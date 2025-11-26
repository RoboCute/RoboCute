#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include "simple_scene.h"
#include "generated/generated.hpp"
#include "rpc_hook_window.h"
// utils.cpp
void warm_up_accel();
void before_frame(rbc::RenderPlugin *render_plugin, rbc::RenderPlugin::PipeCtxStub *pipe_ctx);
void after_frame(
    rbc::RenderPlugin *render_plugin,
    rbc::RenderPlugin::PipeCtxStub *pipe_ctx,
    luisa::compute::TimelineEvent *timeline_event,
    uint64_t signal_fence);
void frame_reset(
    rbc::RenderPlugin *render_plugin,
    rbc::RenderPlugin::PipeCtxStub *pipe_ctx);

int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    log_level_info();
    luisa::fiber::scheduler scheduler;
    RenderDevice render_device;
    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }
    // Create
    render_device.init(
        argv[0],
        backend);
    // let main stream compute-stream
    auto &compute_stream = render_device.lc_async_stream();
    auto present_stream = render_device.lc_device().create_stream(StreamTag::GRAPHICS);
    render_device.set_main_stream(&compute_stream);
    RPCHook rpc_hook{render_device, present_stream};

    auto shader_path = render_device.lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + backend);
    auto &cmdlist = render_device.lc_main_cmd_list();
    vstd::optional<SceneManager> sm;
    const uint2 resolution{1024};
    sm.create(
        render_device.lc_ctx(),
        render_device.lc_device(),
        render_device.lc_async_copy_stream(),
        *render_device.io_service(),
        cmdlist,
        shader_path);
    AssetsManager::init_instance(render_device, sm);
    {
        luisa::fiber::counter counter;
        sm->load_shader(counter);
        warm_up_accel();
        counter.wait();
    }
    auto render_module = DynamicModule::load("rbc_render_plugin");
    auto render_plugin = RBC_LOAD_PLUGIN(render_module, RenderPlugin);
    StateMap pipeline_state_map;
    auto pipe_ctx = render_plugin->create_pipeline_context(pipeline_state_map);
    render_plugin->update_skybox("../sky.bytes", uint2(4096, 2048));
    LUISA_ASSERT(render_plugin->initialize_pipeline({}));
    // init window
    Window window("test graphics", resolution);
    auto swapchain = render_device.lc_device().create_swapchain(
        present_stream,
        SwapchainOption{
            .display = window.native_display(),
            .window = window.native_handle(),
            .size = resolution,
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 2});
    // render loop
    auto timeline_event = render_device.lc_device().create_timeline_event();
    auto stream_evt = render_device.lc_device().create_event();
    uint64_t render_frame_index = 0;
    uint64_t frame_index = 0;
    auto dst_img = render_device.lc_device().create_image<float>(swapchain.backend_storage(), resolution);
    Clock clk;
    double last_frame_time = 0;
    vstd::optional<SimpleScene> simple_scene;
    simple_scene.create();
    // Test FOV
    vstd::optional<float3> cube_move, light_move;
    window.set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        if (action != Action::ACTION_PRESSED) return;
        render_frame_index = 0;
        switch (key) {
            case Key::KEY_SPACE: {
                LUISA_INFO("Reset frame");
            } break;
            case Key::KEY_W: {
                light_move.create();
                *light_move += float3(0, 0.1, 0);
            } break;
            case Key::KEY_S: {
                light_move.create();
                *light_move += float3(0, -0.1, 0);
            } break;
            case Key::KEY_A: {
                light_move.create();
                *light_move += float3(-0.1, 0, 0);
            } break;
            case Key::KEY_D: {
                light_move.create();
                *light_move += float3(0.1, 0, 0);
            } break;
            case Key::KEY_Q: {
                light_move.create();
                *light_move += float3(0, 0, -0.1);
            } break;
            case Key::KEY_E: {
                light_move.create();
                *light_move += float3(0, 0, 0.1);
            } break;
            case Key::KEY_UP: {
                cube_move.create();
                *cube_move += float3(0, 0.1, 0);
            } break;
            case Key::KEY_DOWN: {
                cube_move.create();
                *cube_move += float3(0, -0.1, 0);
            } break;
            case Key::KEY_LEFT: {
                cube_move.create();
                *cube_move += float3(-0.1, 0, 0);
            } break;
            case Key::KEY_RIGHT: {
                cube_move.create();
                *cube_move += float3(0.1, 0, 0);
            } break;
        }
    });
    render_plugin->get_camera(pipe_ctx).fov = radians(80.f);
    while (!window.should_close() && rpc_hook.enabled) {
        AssetsManager::instance()->wake_load_thread();
        window.poll_events();
        if (frame_index > 2) {
            timeline_event.synchronize(frame_index - 2);
        }
        if (rpc_hook.update()) {
            present_stream.synchronize();
            frame_reset(render_plugin, pipe_ctx);
        }
        auto &frame_settings = pipeline_state_map.read_mut<rbc::FrameSettings>();
        frame_settings.render_resolution = dst_img.size();
        frame_settings.display_resolution = dst_img.size();
        frame_settings.dst_img = &dst_img;
        auto time = clk.toc();
        auto delta_time = time - last_frame_time;
        last_frame_time = time;
        frame_settings.delta_time = (float)delta_time;
        frame_settings.time = time;
        frame_settings.frame_index = render_frame_index;
        ++render_frame_index;
        // scene logic
        if (cube_move) {
            simple_scene->move_cube(*cube_move);
            cube_move.destroy();
        }
        if (light_move) {
            simple_scene->move_light(*light_move);
            light_move.destroy();
        }
        // let next-frame's render wait for display
        compute_stream << stream_evt.wait();
        // before render
        before_frame(
            render_plugin,
            pipe_ctx);
        // frame render logic
        after_frame(
            render_plugin,
            pipe_ctx,
            &timeline_event,
            ++frame_index);
        present_stream << timeline_event.wait(frame_index);
        present_stream << swapchain.present(dst_img);
        for (auto &i : rpc_hook.shared_window.swapchains) {
            present_stream << i.second.present(dst_img);
        }
        present_stream << stream_evt.signal();
    }
    rpc_hook.shutdown_remote();
    timeline_event.synchronize(frame_index);
    present_stream.synchronize();
    stream_evt.synchronize();
    // Destroy
    if (!cmdlist.empty()) {
        compute_stream << cmdlist.commit();
    }
    auto pipe_settings_json = pipeline_state_map.serialize_to_json();
    if (pipe_settings_json.data()) {
        LUISA_INFO("{}", luisa::string_view{
                             (char const *)pipe_settings_json.data(),
                             pipe_settings_json.size()});
    }
    compute_stream.synchronize();
    // destroy render-pipeline
    simple_scene.destroy();
    render_plugin->destroy_pipeline_context(pipe_ctx);
    render_plugin->dispose();
    // run last time cycle to eliminate warnings
    // destroy graphics
    AssetsManager::destroy_instance();
    sm->before_rendering(
        cmdlist,
        compute_stream);
    sm->on_frame_end(
        cmdlist,
        compute_stream);
    compute_stream.synchronize();
    sm.destroy();
    render_device.shutdown();
}