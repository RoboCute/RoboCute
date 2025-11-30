#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>

namespace rbc {

struct IApp {
    luisa::unique_ptr<luisa::compute::Context> ctx;

    virtual unsigned int GetDXAdapterLUIDHigh() const = 0;
    virtual unsigned int GetDXAdapterLUIDLow() const = 0;
    virtual void init(const char *program_path, const char *backend_name) = 0;
    virtual uint64_t create_texture(uint width, uint height) = 0;
    virtual void update() = 0;
    virtual void handle_key(luisa::compute::Key key) = 0;
    virtual void *GetStreamNativeHandle() const = 0;
    virtual void *GetDeviceNativeHandle() const = 0;
    virtual ~IApp() {}
};

}// namespace rbc