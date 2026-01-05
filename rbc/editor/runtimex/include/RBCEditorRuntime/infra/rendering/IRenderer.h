#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/gui/input.h>
#include <QtGui/rhi/qrhi.h>

namespace rbc {

struct IRenderer {
    virtual void init(QRhiNativeHandles &) = 0;
    virtual void update() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void handle_key(luisa::compute::Key key, luisa::compute::Action action) = 0;
    virtual void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) = 0;
    virtual void handle_cursor_position(luisa::float2 xy) = 0;

    virtual uint64_t get_present_texture(luisa::uint2 resolution) = 0;
    virtual ~IRenderer() = default;
};

}// namespace rbc