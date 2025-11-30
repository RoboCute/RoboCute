#pragma once
#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/lights.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/vstl/functional.h>
#include <luisa/runtime/swapchain.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>

namespace rbc::my {
struct EventFence {
    TimelineEvent event;
    uint64_t fence_index{};
};
struct GraphicsUtils {
    RenderDevice render_device;
    luisa::string backend_name;
    EventFence compute_event;
    vstd::optional<SceneManager> sm;
    Image<float> dst_image;
    // render
    DynamicModule render_module;
    RenderPlugin *render_plugin{};
    StateMap render_settings;
    RenderPlugin::PipeCtxStub *display_pipe_ctx{};
    vstd::optional<rbc::Lights> lights;
    bool require_reset{false};
    bool display_initialized{false};

    GraphicsUtils();
    void dispose(vstd::function<void()> after_sync = {});
    ~GraphicsUtils();
    void init_device(luisa::string_view program_path, luisa::string_view backend_name);
    void init_graphics(luisa::filesystem::path const &shader_path);
    void init_render();
    void resize_swapchain(uint2 size);
    void init_display(luisa::string_view name, uint2 resolution, bool resizable);
    void reset_frame();
    const Image<float> &GetDestImage() const { return dst_image; }
    bool DisplayInitialized() const { return display_initialized; }

    void tick(vstd::function<void()> before_render = {});
};
}// namespace rbc::my