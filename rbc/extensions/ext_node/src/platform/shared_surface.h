// rbc/extensions/ext_node/src/platform/shared_surface.h
//
// Cross-process GPU texture export for the Electron sharedTexture transport.
//
// The engine renders into a ring of exportable textures; each one is handed to
// the Electron main process ("host") as a platform handle that
// `sharedTexture.importSharedTexture({ textureInfo: { handle } })` accepts:
//
//   win32  : NT HANDLE from ID3D12Device::CreateSharedHandle, DUPLICATED INTO
//            the host process (Electron requires a handle local to the importing
//            process) -> textureInfo.handle.ntHandle            (DONE)
//   macOS  : IOSurfaceRef -> textureInfo.handle.ioSurface        (TODO, vk/metal)
//   linux  : dmabuf planes -> textureInfo.handle.nativePixmap    (TODO, vk)
//
// Everything crossing this boundary is POD / uint64 — no OS or graphics API
// types leak into addon.cpp. Pixel format is fixed to RGBA8 UNorm (byte order
// R,G,B,A), i.e. Electron `pixelFormat: 'rgba'`, LC `PixelStorage::BYTE4`.
#pragma once

#include <cstdint>
#include <string>

namespace rbcnode {

struct SurfaceCaps {
    bool supported = false;
    std::string reason;                 // why not, when !supported
    const char *platform = "";          // "win32" | "darwin" | "linux"
    const char *handle_type = "";       // "ntHandle" | "ioSurface" | "nativePixmap"
    const char *pixel_format = "rgba";  // Electron SharedTextureImportTextureInfo.pixelFormat
};

// One exported texture (one ring slot).
struct ExportedTexture {
    void *native_resource = nullptr;  // backend resource (ID3D12Resource*), owned here, never given to LC
    void *owner = nullptr;            // platform-private allocation owner (ID3D11Texture2D*)
    uint64_t host_handle = 0;         // handle value valid INSIDE the host process
    void *local_handle = nullptr;     // platform-private (our own copy of the handle)
    uint32_t width = 0;
    uint32_t height = 0;
};

// Static capability probe for an RBC backend name ("dx", "vk", ...).
SurfaceCaps surfaceCaps(const std::string &backend);

// Open the host process so handles can be placed into it. Must succeed before
// surfaceCreate. host_pid is the Electron main process id.
bool surfaceAttachHost(uint64_t host_pid, std::string &err);
void surfaceDetachHost();

// Allocate an exportable RGBA8 texture on the engine's native device
// (ID3D12Device* on win32) and export it into the host process.
bool surfaceCreate(void *native_device, uint32_t width, uint32_t height,
                   ExportedTexture &out, std::string &err);

// Release the texture and close the handle in BOTH processes. The caller
// guarantees the GPU is done with it and the host no longer holds a lease.
void surfaceDestroy(ExportedTexture &tex);

// Copy engine: the exported texture is NEVER touched by the engine's own
// graphics API wrapper (LC tracks layouts/barriers it must not apply to a
// cross-process texture). The engine renders into its own staging texture;
// the copier then, on a private queue,
//   waits engine fence (wait_fence, wait_value) -> copies staging -> exported
//   -> signals its own fence; the returned ticket completes when the copy did.
// win32: COPY queue, implicit state promotion/decay, no barriers.
struct SurfaceCopier;
bool surfaceCopierCreate(void *native_device, SurfaceCopier *&out, std::string &err);
// returns 0 on failure (err filled)
uint64_t surfaceCopySubmit(SurfaceCopier *copier, void *wait_fence, uint64_t wait_value,
                           void *staging_resource, const ExportedTexture &dst, std::string &err);
bool surfaceCopyCompleted(SurfaceCopier *copier, uint64_t ticket);
void surfaceCopierWaitIdle(SurfaceCopier *copier);
void surfaceCopierDestroy(SurfaceCopier *&copier);

}// namespace rbcnode
