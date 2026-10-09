// rbc/extensions/ext_node/src/platform/shared_surface_win32.cpp
//
// Win32 / D3D12 implementation of platform/shared_surface.h.
//
//   D3D11 CreateTexture2D(RGBA8, SRV|RT, MISC_SHARED|SHARED_NTHANDLE) on the
//   engine's adapter (LUID) -> IDXGIResource1::CreateSharedHandle (NT HANDLE)
//     -> ID3D12Device::OpenSharedHandle (write side, copy queue only)
//     -> DuplicateHandle into the Electron main process
//   SurfaceCopier: private COPY queue, waits the engine fence, CopyResource
//     staging -> exported, signals its own fence.
//
// Electron's importSharedTexture duplicates the handle it is given again
// (scoped to the imported SharedImage), so the copy we place in the host stays
// owned by us; surfaceDestroy closes it remotely with DUPLICATE_CLOSE_SOURCE —
// the main process never needs native code.
//
// rgba textures carry no keyed mutex: cross-device ordering is guaranteed by
// the protocol instead (engine leases a slot only after the copier fence
// passed; the host returns the lease only after the renderer's GPU work on the
// texture completed).

#include "shared_surface.h"

#ifdef _WIN32

#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdio>
#include <vector>

namespace rbcnode {

namespace {
HANDLE g_host_process = nullptr;

std::string hresultMessage(const char *what, HRESULT hr) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lx)", what, static_cast<unsigned long>(hr));
    return buf;
}
}// namespace

SurfaceCaps surfaceCaps(const std::string &backend) {
    SurfaceCaps caps{};
    caps.platform = "win32";
    caps.handle_type = "ntHandle";
    if (backend != "dx") {
        caps.supported = false;
        caps.reason = "backend '" + backend +
                      "' cannot export D3D NT handles on Windows; set RBC_BACKEND=dx";
        return caps;
    }
    caps.supported = true;
    return caps;
}

bool surfaceAttachHost(uint64_t host_pid, std::string &err) {
    surfaceDetachHost();
    if (host_pid == 0) {
        err = "host pid is 0";
        return false;
    }
    g_host_process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, static_cast<DWORD>(host_pid));
    if (!g_host_process) {
        char buf[128];
        snprintf(buf, sizeof(buf), "OpenProcess(PROCESS_DUP_HANDLE, pid=%llu) failed (err=%lu)",
                 static_cast<unsigned long long>(host_pid), GetLastError());
        err = buf;
        return false;
    }
    return true;
}

void surfaceDetachHost() {
    if (g_host_process) {
        CloseHandle(g_host_process);
        g_host_process = nullptr;
    }
}

// Exported textures are allocated by D3D11 on the engine's adapter — the
// layout Chromium's D3DImageBacking itself produces and consumes — and only
// opened in D3D12 for the copy-queue write. D3D12-allocated shared textures
// (with or without SIMULTANEOUS_ACCESS) were observed to fault Chromium's GPU
// process (NVIDIA Xid 13) when sampled by WebGPU.
ID3D11Device *g_d3d11 = nullptr;

bool ensureD3D11(ID3D12Device *device, std::string &err) {
    if (g_d3d11) return true;
    LUID luid = device->GetAdapterLuid();
    IDXGIFactory4 *factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void **>(&factory));
    if (FAILED(hr)) {
        err = hresultMessage("CreateDXGIFactory1", hr);
        return false;
    }
    IDXGIAdapter1 *adapter = nullptr;
    hr = factory->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter1), reinterpret_cast<void **>(&adapter));
    factory->Release();
    if (FAILED(hr)) {
        err = hresultMessage("EnumAdapterByLuid", hr);
        return false;
    }
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                           &g_d3d11, nullptr, nullptr);
    adapter->Release();
    if (FAILED(hr)) {
        err = hresultMessage("D3D11CreateDevice", hr);
        return false;
    }
    return true;
}

bool surfaceCreate(void *native_device, uint32_t width, uint32_t height,
                   ExportedTexture &out, std::string &err) {
    out = {};
    auto *device = static_cast<ID3D12Device *>(native_device);
    if (!device) {
        err = "native D3D12 device is null";
        return false;
    }
    if (!g_host_process) {
        err = "host process not attached";
        return false;
    }
    if (!ensureD3D11(device, err)) return false;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    // NT handle, no keyed mutex (Electron: rgba imports carry none)
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    ID3D11Texture2D *tex11 = nullptr;
    HRESULT hr = g_d3d11->CreateTexture2D(&desc, nullptr, &tex11);
    if (FAILED(hr)) {
        err = hresultMessage("D3D11 CreateTexture2D(shared RGBA8)", hr);
        return false;
    }

    HANDLE local = nullptr;
    IDXGIResource1 *dxgi = nullptr;
    hr = tex11->QueryInterface(__uuidof(IDXGIResource1), reinterpret_cast<void **>(&dxgi));
    if (SUCCEEDED(hr)) {
        hr = dxgi->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                                      nullptr, &local);
        dxgi->Release();
    }
    if (FAILED(hr)) {
        tex11->Release();
        err = hresultMessage("IDXGIResource1::CreateSharedHandle", hr);
        return false;
    }

    ID3D12Resource *resource = nullptr;
    hr = device->OpenSharedHandle(local, __uuidof(ID3D12Resource), reinterpret_cast<void **>(&resource));
    if (FAILED(hr)) {
        CloseHandle(local);
        tex11->Release();
        err = hresultMessage("ID3D12Device::OpenSharedHandle", hr);
        return false;
    }

    HANDLE remote = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), local, g_host_process, &remote, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        char buf[96];
        snprintf(buf, sizeof(buf), "DuplicateHandle into host failed (err=%lu)", GetLastError());
        resource->Release();
        CloseHandle(local);
        tex11->Release();
        err = buf;
        return false;
    }

    out.native_resource = resource;
    out.owner = tex11;
    out.local_handle = local;
    out.host_handle = reinterpret_cast<uint64_t>(remote);
    out.width = width;
    out.height = height;
    return true;
}

void surfaceDestroy(ExportedTexture &tex) {
    if (tex.host_handle && g_host_process) {
        // close our copy inside the host process (no target process needed)
        DuplicateHandle(g_host_process, reinterpret_cast<HANDLE>(tex.host_handle), nullptr,
                        nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
    }
    if (tex.local_handle) CloseHandle(static_cast<HANDLE>(tex.local_handle));
    if (tex.native_resource) static_cast<ID3D12Resource *>(tex.native_resource)->Release();
    if (tex.owner) static_cast<ID3D11Texture2D *>(tex.owner)->Release();
    tex = {};
}

// ---------------------------------------------------------------------------
// Copier: private D3D12 COPY queue. Both textures are in the COMMON layout
// when the copy starts (LC restores its textures to COMMON at the end of
// every command list; the exported texture never leaves COMMON), so implicit
// promotion to COPY_SOURCE / COPY_DEST and decay back to COMMON cover all
// state handling — no barriers are recorded here.
// ---------------------------------------------------------------------------
struct CopyContext {
    ID3D12CommandAllocator *allocator = nullptr;
    ID3D12GraphicsCommandList *list = nullptr;
    uint64_t ticket = 0;  // copier fence value that retires this context
};

struct SurfaceCopier {
    ID3D12Device *device = nullptr;
    ID3D12CommandQueue *queue = nullptr;
    ID3D12Fence *fence = nullptr;
    HANDLE idle_event = nullptr;
    uint64_t next_ticket = 0;
    std::vector<CopyContext> contexts;
};

bool surfaceCopierCreate(void *native_device, SurfaceCopier *&out, std::string &err) {
    out = nullptr;
    auto *device = static_cast<ID3D12Device *>(native_device);
    if (!device) {
        err = "native D3D12 device is null";
        return false;
    }
    auto *c = new SurfaceCopier{};
    c->device = device;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    HRESULT hr = device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&c->queue));
    if (SUCCEEDED(hr))
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void **>(&c->fence));
    if (FAILED(hr)) {
        err = hresultMessage("create copy queue/fence", hr);
        surfaceCopierDestroy(c);
        return false;
    }
    c->idle_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    out = c;
    return true;
}

uint64_t surfaceCopySubmit(SurfaceCopier *c, void *wait_fence, uint64_t wait_value,
                           void *staging_resource, const ExportedTexture &dst, std::string &err) {
    CopyContext *ctx = nullptr;
    const uint64_t done = c->fence->GetCompletedValue();
    for (auto &x : c->contexts)
        if (x.ticket <= done) { ctx = &x; break; }
    HRESULT hr = S_OK;
    if (!ctx) {
        CopyContext fresh{};
        hr = c->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, __uuidof(ID3D12CommandAllocator),
                                               reinterpret_cast<void **>(&fresh.allocator));
        if (SUCCEEDED(hr))
            hr = c->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, fresh.allocator, nullptr,
                                              __uuidof(ID3D12GraphicsCommandList),
                                              reinterpret_cast<void **>(&fresh.list));
        if (FAILED(hr)) {
            if (fresh.allocator) fresh.allocator->Release();
            err = hresultMessage("create copy command list", hr);
            return 0;
        }
        fresh.list->Close();
        c->contexts.push_back(fresh);
        ctx = &c->contexts.back();
    }
    hr = ctx->allocator->Reset();
    if (SUCCEEDED(hr)) hr = ctx->list->Reset(ctx->allocator, nullptr);
    if (FAILED(hr)) {
        err = hresultMessage("reset copy command list", hr);
        return 0;
    }
    ctx->list->CopyResource(static_cast<ID3D12Resource *>(dst.native_resource),
                            static_cast<ID3D12Resource *>(staging_resource));
    hr = ctx->list->Close();
    if (FAILED(hr)) {
        err = hresultMessage("close copy command list", hr);
        return 0;
    }
    // GPU-side wait: the engine's blit into the staging texture has finished
    hr = c->queue->Wait(static_cast<ID3D12Fence *>(wait_fence), wait_value);
    if (FAILED(hr)) {
        err = hresultMessage("copy queue Wait", hr);
        return 0;
    }
    ID3D12CommandList *lists[] = {ctx->list};
    c->queue->ExecuteCommandLists(1, lists);
    ctx->ticket = ++c->next_ticket;
    hr = c->queue->Signal(c->fence, ctx->ticket);
    if (FAILED(hr)) {
        err = hresultMessage("copy queue Signal", hr);
        return 0;
    }
    return ctx->ticket;
}

bool surfaceCopyCompleted(SurfaceCopier *c, uint64_t ticket) {
    return c->fence->GetCompletedValue() >= ticket;
}

void surfaceCopierWaitIdle(SurfaceCopier *c) {
    if (!c || !c->fence || c->fence->GetCompletedValue() >= c->next_ticket) return;
    c->fence->SetEventOnCompletion(c->next_ticket, c->idle_event);
    WaitForSingleObject(c->idle_event, INFINITE);
}

void surfaceCopierDestroy(SurfaceCopier *&c) {
    if (!c) return;
    surfaceCopierWaitIdle(c);
    for (auto &x : c->contexts) {
        if (x.list) x.list->Release();
        if (x.allocator) x.allocator->Release();
    }
    if (c->fence) c->fence->Release();
    if (c->queue) c->queue->Release();
    if (c->idle_event) CloseHandle(c->idle_event);
    delete c;
    c = nullptr;
}

}// namespace rbcnode

#endif// _WIN32
