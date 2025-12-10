#pragma once
#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/rhi/resource.h>
#include <luisa/backends/ext/native_resource_ext_interface.h>
#include "RBCEditor/app.h"
// #include "RBCEditor/runtime/RenderUtils.h"
#include <rbc_app/graphics_utils.h>
#include <rbc_app/camera_controller.h>
#include "RBCEditor/runtime/RenderScene.h"

namespace rbc {

enum struct MouseStage {
    None,
    Dragging,
    Clicking,
    Clicked
};

struct VisApp : public IApp {
    luisa::uint2 resolution;
    luisa::uint2 dx_adaptor_luid;

    luisa::fiber::scheduler scheduler;
    GraphicsUtils utils;

    uint64_t frame_index = 0;
    double last_frame_time = 0;
    luisa::Clock clk;
    bool reset = false;
    bool dst_image_reseted = false;

    MouseStage mouse_stage{MouseStage::None};
    CameraController::Input camera_input;
    float2 start_uv, end_uv;
    CameraController cam_controller;
    luisa::vector<uint> dragged_object_ids;

public:
    [[nodiscard]] unsigned int GetDXAdapterLUIDHigh() const override { return dx_adaptor_luid.x; }
    [[nodiscard]] unsigned int GetDXAdapterLUIDLow() const override { return dx_adaptor_luid.y; }
    void init(const char *program_path, const char *backend_name) override;
    uint64_t create_texture(uint width, uint height) override;
    void update() override;

    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;

    [[nodiscard]] void *GetStreamNativeHandle() const override {
        LUISA_ASSERT(utils.present_stream());
        return utils.present_stream().native_handle();
    }
    [[nodiscard]] void *GetDeviceNativeHandle() const override { return RenderDevice::instance().lc_device().native_handle(); }
    ~VisApp();
};

}// namespace rbc
