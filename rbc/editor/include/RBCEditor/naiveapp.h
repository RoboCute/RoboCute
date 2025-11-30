#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/rhi/resource.h>
#include <luisa/backends/ext/native_resource_ext_interface.h>
#include "RBCEditor/app.h"

namespace rbc {

struct NaiveApp : public IApp {
    luisa::compute::Device device;
    luisa::compute::Stream stream;
    luisa::compute::CommandList cmd_list;

    luisa::compute::Image<float> dummy_image;

    luisa::compute::DeviceConfigExt *device_config_ext{};

    luisa::compute::Shader<2, luisa::compute::Image<float>> clear_shader;
    luisa::compute::Shader<2, luisa::compute::Image<float>, float, luisa::compute::float2> draw_shader;

    luisa::uint2 resolution;
    luisa::Clock clk;
    luisa::uint2 dx_adaptor_luid;

    unsigned int GetDXAdapterLUIDHigh() const override { return dx_adaptor_luid.x; }
    unsigned int GetDXAdapterLUIDLow() const override { return dx_adaptor_luid.y; }
    void init(luisa::compute::Context &ctx, const char *backend_name) override;
    uint64_t create_texture(uint width, uint height) override;
    void update() override;
    void handle_key(luisa::compute::Key key) override;
    void *GetStreamNativeHandle() const override { return stream.native_handle(); }
    void *GetDeviceNativeHandle() const override { return device.native_handle(); }
    ~NaiveApp();
};

}// namespace rbc
