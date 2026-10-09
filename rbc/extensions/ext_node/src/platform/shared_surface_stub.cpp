// rbc/extensions/ext_node/src/platform/shared_surface_stub.cpp
//
// Platforms without a texture exporter yet. capabilities() reports the exact
// reason and the Electron UI shows "unsupported" — there is intentionally no
// CPU readback fallback.
//   macOS : IOSurface-backed texture (LC metal / vk + VK_EXT_metal_objects)
//   Linux : dmabuf export (LC vk + VK_EXT_external_memory_dma_buf)

#include "shared_surface.h"

#ifndef _WIN32

namespace rbcnode {

SurfaceCaps surfaceCaps(const std::string &) {
    SurfaceCaps caps{};
#if defined(__APPLE__)
    caps.platform = "darwin";
    caps.handle_type = "ioSurface";
    caps.reason = "IOSurface texture export is not implemented yet (see samples/electron/FUTURE.md)";
#else
    caps.platform = "linux";
    caps.handle_type = "nativePixmap";
    caps.reason = "dmabuf texture export is not implemented yet (see samples/electron/FUTURE.md)";
#endif
    caps.supported = false;
    return caps;
}

bool surfaceAttachHost(uint64_t, std::string &err) {
    err = surfaceCaps({}).reason;
    return false;
}

void surfaceDetachHost() {}

bool surfaceCreate(void *, uint32_t, uint32_t, ExportedTexture &, std::string &err) {
    err = surfaceCaps({}).reason;
    return false;
}

void surfaceDestroy(ExportedTexture &tex) { tex = {}; }

struct SurfaceCopier {};

bool surfaceCopierCreate(void *, SurfaceCopier *&out, std::string &err) {
    out = nullptr;
    err = surfaceCaps({}).reason;
    return false;
}

uint64_t surfaceCopySubmit(SurfaceCopier *, void *, uint64_t, void *, const ExportedTexture &, std::string &err) {
    err = surfaceCaps({}).reason;
    return 0;
}

bool surfaceCopyCompleted(SurfaceCopier *, uint64_t) { return false; }
void surfaceCopierWaitIdle(SurfaceCopier *) {}
void surfaceCopierDestroy(SurfaceCopier *&copier) { copier = nullptr; }

}// namespace rbcnode

#endif// !_WIN32
