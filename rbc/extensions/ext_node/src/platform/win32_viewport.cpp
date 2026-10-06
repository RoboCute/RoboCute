// rbc/extensions/ext_node/src/platform/win32_viewport.cpp
//
// Win32 implementation of the shared-present viewport: a WS_CHILD window
// parented (cross-process) to the Electron browser window, presented via a
// DXGI swapchain created by RBC on our HWND. Mouse input is handled natively
// here — same deltas as the JS gesture path.

#include "viewport.h"

#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>

#include <chrono>
#include <cstdio>

namespace rbcnode {

namespace {

// One viewport per process (see viewport.h).
Viewport *g_vp = nullptr;
CameraSink g_sink{};

// Layout contract shared with main.js / renderer (CSS pixels): the viewport
// covers the parent client area minus the toolbar band and the sidebar.
ViewportRect computeRect(HWND parent, uint32_t inset_top, uint32_t inset_right) {
    RECT rc{};
    GetClientRect(parent, &rc);
    ViewportRect r{};
    r.x = 0;
    r.y = (int)inset_top;
    r.w = (int)rc.right - (int)inset_right;
    r.h = (int)rc.bottom - (int)inset_top;
    if (r.w < 64) r.w = 64;
    if (r.h < 64) r.h = 64;
    return r;
}

LRESULT CALLBACK viewportWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            g_vp->dragging = true;
            g_vp->last_x = GET_X_LPARAM(lp);
            g_vp->last_y = GET_Y_LPARAM(lp);
            SetCapture(hwnd);
            return 0;
        case WM_MOUSEMOVE:
            if (g_vp->dragging) {
                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                // same sensitivity as renderer.js (dx * 0.005)
                if (g_sink.rotate)
                    g_sink.rotate((x - g_vp->last_x) * 0.005f,
                                  (y - g_vp->last_y) * 0.005f);
                g_vp->last_x = x;
                g_vp->last_y = y;
            }
            return 0;
        case WM_LBUTTONUP:
            g_vp->dragging = false;
            ReleaseCapture();
            return 0;
        case WM_MOUSEWHEEL: {
            // wheel up (positive delta) dollies in, like renderer.js sign convention
            if (g_sink.zoom)
                g_sink.zoom(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -0.15f : 0.15f);
            return 0;
        }
        default:
            return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

void viewportThreadFn(Viewport &vp) {
    HINSTANCE inst = GetModuleHandleA(nullptr);
    WNDCLASSA wc{};
    wc.lpfnWndProc = viewportWndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = "RBCSharedViewport";
    if (!RegisterClassA(&wc)) {
        fprintf(stderr, "[rbc_ext_node] viewport RegisterClass failed: %lu\n", GetLastError());
        return;
    }
    ViewportRect vr = computeRect(reinterpret_cast<HWND>(vp.parent_handle),
                                  vp.inset_top, vp.inset_right);
    HWND hwnd = CreateWindowExA(
        0, "RBCSharedViewport", nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        vr.x, vr.y, vr.w, vr.h,
        reinterpret_cast<HWND>(vp.parent_handle), nullptr, inst, nullptr);
    if (!hwnd) {
        fprintf(stderr, "[rbc_ext_node] viewport CreateWindow failed: %lu\n", GetLastError());
        return;
    }
    // Chromium keeps its own compositor child ("Intermediate D3D Window") over
    // the client area; start on top of the sibling stack. The engine loop
    // re-asserts this periodically (viewportRaise).
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    vp.viewport_handle = reinterpret_cast<uint64_t>(hwnd);
    fprintf(stderr, "[rbc_ext_node] shared viewport hwnd=%p (%dx%d @%d,%d, insets t%u r%u)\n",
            (void *)hwnd, vr.w, vr.h, vr.x, vr.y, vp.inset_top, vp.inset_right);
    while (!vp.quit.load()) {
        MSG msg{};
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

}// namespace

void setCameraSink(const CameraSink &sink) { g_sink = sink; }

bool viewportStart(Viewport &vp) {
    g_vp = &vp;
    vp.thread = std::thread(viewportThreadFn, std::ref(vp));
    return true;  // creation success is observed via vp.viewport_handle
}

void viewportStop(Viewport &vp) {
    vp.quit = true;
    if (vp.thread.joinable()) vp.thread.join();
}

bool viewportParentAlive(const Viewport &vp) {
    RECT rc{};
    return GetClientRect(reinterpret_cast<HWND>(vp.parent_handle), &rc) &&
           rc.right > 0 && rc.bottom > 0;
}

ViewportRect viewportRect(const Viewport &vp) {
    return computeRect(reinterpret_cast<HWND>(vp.parent_handle),
                       vp.inset_top, vp.inset_right);
}

void viewportMove(Viewport &vp, const ViewportRect &r) {
    MoveWindow(reinterpret_cast<HWND>(vp.viewport_handle),
               r.x, r.y, r.w, r.h, TRUE);
    viewportRaise(vp);
}

void viewportRaise(Viewport &vp) {
    SetWindowPos(reinterpret_cast<HWND>(vp.viewport_handle), HWND_TOP,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

}// namespace rbcnode

#endif// _WIN32
