#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/gui/input.h>
#include <QtGui/rhi/qrhi.h>

namespace rbc {

// The Luisa Rendrerer Interface
struct IRenderer {
    virtual void init(QRhiNativeHandles &) = 0;
    virtual void update() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void handle_key(luisa::compute::Key key) = 0;
    virtual uint64_t get_present_texture(luisa::uint2 resolution) = 0;
protected:
    ~IRenderer() = default;
};

}// namespace rbc