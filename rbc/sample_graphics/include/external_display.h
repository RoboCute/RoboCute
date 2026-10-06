#pragma once
// External native display embedding (e.g. Electron shared-texture present).
//
// Lets a host application lend an OS-native window handle (HWND on Windows)
// to RBCContext::init_display/reset_view WITHOUT RBC creating its own GLFW
// window (create_window = false). The handle is stored process-wide; the
// intended call sequence from the host is:
//
//   rbc::set_external_display_handles(display, hwnd);   // before init_display
//   rbc::RBCContext::init_display(ctx, name, size,
//                                 /*create_window=*/false, ...);
//   // later, on host window resize:
//   rbc::RBCContext::reset_view(ctx, new_size);
//
// The swapchain is then created on the borrowed HWND and every tick() blits
// the render target to it — a GPU-direct present, no CPU readback.
// Pass luisa::compute::invalid_resource_handle for both to clear.

#include <cstdint>

#ifndef SAMPLE_GRAPHICS_API
#define SAMPLE_GRAPHICS_API
#endif

namespace rbc {

SAMPLE_GRAPHICS_API void set_external_display_handles(uint64_t display, uint64_t window);
SAMPLE_GRAPHICS_API void get_external_display_handles(uint64_t &display, uint64_t &window);

}// namespace rbc
