// rbc/extensions/ext_node/src/platform/cocoa_viewport.cpp
//
// macOS STUB — intentionally unimplemented for the prototype phase.
//
// Planned real implementation (see samples/electron/FUTURE.md, route A):
//   * rename this file to cocoa_viewport.mm once it needs Objective-C++;
//   * create a child NSView inside Electron's window contentView, forward its
//     mouse events to the CameraSink (acceptsFirstResponder + NSEvent pans);
//   * pass the parent NSWindow*/NSView* through Viewport handles — LC's vk
//     backend already accepts it and builds a VkMacOSSurfaceCreateInfoMVK
//     (thirdparty/LuisaCompute/src/backends/vk/swapchain.cpp, APPLE branch);
//   * requires the vk backend (MoltenVK) instead of dx.

#include "viewport.h"

#if defined(__APPLE__)

#include <cstdio>

namespace rbcnode {

void setCameraSink(const CameraSink &) {}

bool viewportStart(Viewport &) {
    fprintf(stderr, "[rbc_ext_node] shared viewport: macOS not implemented yet "
                    "(see samples/electron/FUTURE.md)\n");
    return false;
}

void viewportStop(Viewport &) {}
bool viewportParentAlive(const Viewport &) { return false; }
ViewportRect viewportRect(const Viewport &) { return {}; }
void viewportMove(Viewport &, const ViewportRect &) {}
void viewportRaise(Viewport &) {}

}// namespace rbcnode

#endif// __APPLE__
