#pragma once

#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>

namespace rbc {

struct IApp {
    using uint = luisa::compute::uint;

    luisa::unique_ptr<luisa::compute::Context> ctx;

    virtual unsigned int GetDXAdapterLUIDHigh() const = 0;
    virtual unsigned int GetDXAdapterLUIDLow() const = 0;
    virtual void init(const char *program_path, const char *backend_name) = 0;
    virtual uint64_t create_texture(uint width, uint height) = 0;
    virtual void update() = 0;
    virtual void handle_key(luisa::compute::Key key, luisa::compute::Action action) = 0;
    virtual void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) {}
    virtual void handle_cursor_position(luisa::float2 xy) {}
    virtual void *GetStreamNativeHandle() const = 0;
    virtual void *GetDeviceNativeHandle() const = 0;
    virtual ~IApp() {}
};

}// namespace rbc