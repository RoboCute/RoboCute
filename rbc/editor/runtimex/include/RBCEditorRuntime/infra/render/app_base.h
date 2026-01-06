#pragma once

#include <rbc_app/graphics_utils.h>
#include <rbc_app/camera_controller.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <QtGui/rhi/qrhi.h>

namespace rbc {

enum struct RenderMode {
    Editor,
    PBR
};

// Binding Wrapper for QRhi
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

// Background Implementation App
struct IRenderApp {
    using uint = luisa::compute::uint;

    luisa::unique_ptr<luisa::compute::Context> ctx;
    virtual unsigned int GetDXAdapterLUIDHigh() const = 0;
    virtual unsigned int GetDXAdapterLUIDLow() const = 0;

    virtual void init(const char *program_path, const char *backend_name) = 0;
    virtual void update() = 0;

    virtual void handle_key(
        luisa::compute::Key key,
        luisa::compute::Action action) = 0;
    virtual void handle_mouse(
        luisa::compute::MouseButton button,
        luisa::compute::Action action,
        luisa::float2 xy) {}

    virtual void handle_cursor_position(luisa::float2 xy) {}
    virtual uint64_t create_texture(uint width, uint height) = 0;
    virtual void *GetStreamNativeHandle() const = 0;
    virtual void *GetDeviceNativeHandle() const = 0;
    virtual ~IRenderApp() {}
};

struct RBC_EDITOR_RUNTIME_API RenderAppBase : public IRenderApp {
    using uint2 = luisa::uint2;
    uint2 resolution;
    uint2 dx_adapter_luid;
    luisa::fiber::scheduler scheduler;
    GraphicsUtils utils;
    uint64_t frame_index = 0;
    double last_frame_time = 0;
    luisa::Clock clk;
    bool reset = false;
    // Camera Control
    CameraController::Input camera_input;
    CameraController cam_controller;
public:
    [[nodiscard]] unsigned int GetDXAdapterLUIDHigh() const override { return dx_adapter_luid.x; }
    [[nodiscard]] unsigned int GetDXAdapterLUIDLow() const override { return dx_adapter_luid.y; }
    [[nodiscard]] void *GetStreamNativeHandle() const override;
    [[nodiscard]] void *GetDeviceNativeHandle() const override;
    [[nodiscard]] virtual RenderMode getRenderMode() const = 0;
    void init(const char *program_path, const char *backend_name) override;
    uint64_t create_texture(uint width, uint height) override;
    void handle_reset();
    void prepare_dx_states();
    ~RenderAppBase() override;
    // interface
    virtual void update_camera(float delta_time);
    virtual void dispose();

protected:
    virtual void on_init() {}
};

}// namespace rbc