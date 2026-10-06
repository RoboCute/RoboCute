// rbc/extensions/ext_node/src/platform/viewport.h
//
// Platform abstraction for the shared-present viewport: a native child window
// borrowed from the host (Electron) whose handle is handed to RBC, which
// creates a GPU swapchain on it (DXGI swapchain on win32, vk surface on
// mac/linux — see samples/electron/FUTURE.md).
//
// Everything crossing this boundary is a plain uint64 / POD — no OS headers,
// no HANDLE / NSWindow* / Window types leak into addon.cpp. One viewport per
// process (the engine demo is a singleton), which the implementations rely on.
//
// Implementations:
//   win32_viewport.cpp — HWND child + WndProc camera input  (DONE)
//   cocoa_viewport.cpp — stub; real impl will be a .mm      (TODO: macOS)
//   x11_viewport.cpp   — stub                               (TODO: Linux)
#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace rbcnode {

// Placement of the viewport inside the parent window's client area
// (already inset-adjusted; platform code consumes it verbatim).
struct ViewportRect {
    int x, y, w, h;
};

// Native camera input, translated by the platform layer into the same
// deltas the JS gesture path produces. Registered once before viewportStart.
struct CameraSink {
    void (*rotate)(float yaw, float pitch);
    void (*zoom)(float dz);
};
void setCameraSink(const CameraSink &sink);

// Shared viewport state. `parent_handle` / `viewport_handle` are opaque
// platform handles (HWND / NSWindow* / Xlib Window as uint64). The platform
// layer may read the public fields; drag state is platform-private scratch
// (kept here because there is exactly one viewport per process).
struct Viewport {
    uint64_t parent_handle = 0;    // host window (Electron), set before start
    uint64_t viewport_handle = 0;  // our child window, set by viewportStart
    uint32_t inset_top = 0;        // toolbar band, client px
    uint32_t inset_right = 0;      // sidebar width, client px
    std::atomic<bool> quit{false};
    std::thread thread;
    bool dragging = false;
    int last_x = 0, last_y = 0;
};

// Create the child window on parent_handle and pump its messages on
// vp.thread until viewportStop. Must set vp.viewport_handle before returning
// (engine thread polls it). Returns false if the platform has no
// implementation (stub platforms) or creation failed.
bool viewportStart(Viewport &vp);
// Signal quit and join vp.thread.
void viewportStop(Viewport &vp);
// False once the parent window is gone (engine should stop).
bool viewportParentAlive(const Viewport &vp);
// Current placement within the parent client area honoring the insets
// (clamped to a sane minimum so swapchain recreation never gets a 0 size).
ViewportRect viewportRect(const Viewport &vp);
// Reposition/resize the viewport (engine-thread resize detection).
void viewportMove(Viewport &vp, const ViewportRect &r);
// Re-assert z-order above the host's own compositor surface. Chromium keeps
// an "Intermediate D3D Window" over its client area; the viewport must stay
// on top of the sibling stack or it paints underneath (see README pitfall 2).
void viewportRaise(Viewport &vp);

}// namespace rbcnode
