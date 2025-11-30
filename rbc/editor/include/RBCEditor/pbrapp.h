#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/rhi/resource.h>
#include <luisa/backends/ext/native_resource_ext_interface.h>
#include "RBCEditor/app.h"
// #include "RBCEditor/runtime/RenderUtils.h"
#include "RBCEditor/runtime/MyRenderUtils.h"
#include "RBCEditor/runtime/RenderScene.h"

namespace rbc {

struct PBRApp : public IApp {
    luisa::uint2 resolution;
    luisa::uint2 dx_adaptor_luid;

    luisa::fiber::scheduler scheduler;
    my::GraphicsUtils utils;

    uint64_t frame_index = 0;
    double last_frame_time = 0;
    luisa::Clock clk;
    vstd::optional<rbc::SimpleScene> simple_scene;
    vstd::optional<float3> cube_move, light_move;
    bool reset = false;

public:
    [[nodiscard]] unsigned int GetDXAdapterLUIDHigh() const override { return dx_adaptor_luid.x; }
    [[nodiscard]] unsigned int GetDXAdapterLUIDLow() const override { return dx_adaptor_luid.y; }
    void init(const char *program_path, const char *backend_name) override;
    uint64_t create_texture(uint width, uint height) override;
    void update() override;
    void handle_key(luisa::compute::Key key) override;
    [[nodiscard]] void *GetStreamNativeHandle() const override { return utils.render_device.lc_main_stream().native_handle(); }
    [[nodiscard]] void *GetDeviceNativeHandle() const override { return utils.render_device.lc_main_stream().native_handle(); }
    ~PBRApp();
};

}// namespace rbc
