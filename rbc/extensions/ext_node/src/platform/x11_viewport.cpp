// rbc/extensions/ext_node/src/platform/x11_viewport.cpp
//
// Linux STUB — intentionally unimplemented for the prototype phase.
//
// Planned real implementation (see samples/electron/FUTURE.md, route A):
//   * create an X11 child window (XCreateWindow) under Electron's toplevel,
//     forward XInput2 pointer events to the CameraSink;
//   * pass the Xlib Window through Viewport handles — LC's vk backend already
//     builds a VkXlibSurfaceCreateInfoKHR on it
//     (thirdparty/LuisaCompute/src/backends/vk/swapchain.cpp);
//   * Wayland needs a separate dmabuf-based route; X11 first.

#include "viewport.h"

#if defined(__linux__)

#include <cstdio>

namespace rbcnode {

void setCameraSink(const CameraSink &) {}

bool viewportStart(Viewport &) {
    fprintf(stderr, "[rbc_ext_node] shared viewport: Linux not implemented yet "
                    "(see samples/electron/FUTURE.md)\n");
    return false;
}

void viewportStop(Viewport &) {}
bool viewportParentAlive(const Viewport &) { return false; }
ViewportRect viewportRect(const Viewport &) { return {}; }
void viewportMove(Viewport &, const ViewportRect &) {}
void viewportRaise(Viewport &) {}

}// namespace rbcnode

#endif// __linux__
