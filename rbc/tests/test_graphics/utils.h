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
namespace rbc {
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

struct EventFence {
    TimelineEvent event;
    uint64_t fence_index{};
};
struct GraphicsUtils {
    RenderDevice render_device;
    luisa::string backend_name;
    EventFence compute_event;
    vstd::optional<SceneManager> sm;
    // present
    Stream present_stream;
    EventFence present_event;
    vstd::optional<Window> window;
    Swapchain swapchain;
    Image<float> dst_image;
    // render
    DynamicModule render_module;
    RenderPlugin *render_plugin{};
    StateMap render_settings;
    RenderPlugin::PipeCtxStub *display_pipe_ctx{};
    vstd::optional<rbc::Lights> lights;
    bool require_reset{false};

    GraphicsUtils();
    void dispose(vstd::function<void()> after_sync = {});
    ~GraphicsUtils();
    void init_device(luisa::string_view program_path, luisa::string_view backend_name);
    void init_graphics(luisa::filesystem::path const &shader_path);
    void init_render();
    void resize_swapchain(uint2 size);
    void init_display(luisa::string_view name, uint2 resolution, bool resizable);
    void reset_frame();
    bool should_close();
    void tick(vstd::function<void()> before_render = {});
    static void openpbr_json_ser(JsonSerializer &json_ser, material::OpenPBR const &mat);
    static void openpbr_json_deser(JsonDeSerializer &json_deser, material::OpenPBR &mat);
    static void openpbr_json_ser(JsonSerializer &json_ser, material::Unlit const &mat);
    static void openpbr_json_deser(JsonDeSerializer &json_deser, material::Unlit &mat);
};
}// namespace rbc