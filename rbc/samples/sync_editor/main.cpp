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

#include "http_client.h"
#include "scene_sync.h"
#include "editor_scene.h"

void warm_up_accel();
void before_frame(rbc::RenderPlugin *render_plugin, rbc::RenderPlugin::PipeCtxStub *pipe_ctx);
void after_frame(
    rbc::RenderPlugin *render_plugin,
    rbc::RenderPlugin::PipeCtxStub *pipe_ctx,
    luisa::compute::TimelineEvent *timeline_event,
    uint64_t signal_fence);

int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;

    log_level_info();

    // Parse command line args
    luisa::string backend = "dx";
    luisa::string server_url = "http://127.0.0.1:5555";
    if (argc >= 2) {
        backend = argv[1];
    }
    if (argc >= 3) {
        server_url = argv[2];
    }

    LUISA_INFO("=== RoboCute Editor ===");
    LUISA_INFO("Backend: {}", backend);
    LUISA_INFO("Server: {}", server_url);

    // Initialize HTTP client
    HttpClient http_client;
    http_client.set_server_url(server_url);

    // Register with server
    luisa::string editor_id = "editor_001";
    if (!http_client.register_editor(editor_id)) {
        LUISA_WARNING("Failed to register with server, but continuing anyway...");
    } else {
        LUISA_INFO("Registered with server as: {}", editor_id);
    }

    // Initialize scene sync
    SceneSync scene_sync;

    // Initialize render device
    luisa::fiber::scheduler scheduler;
    RenderDevice render_device;

    render_device.init(argv[0], backend);

    auto &compute_stream = render_device.lc_async_stream();
    auto present_stream = render_device.lc_device().create_stream(StreamTag::GRAPHICS);
    render_device.set_main_stream(&compute_stream);

    auto shader_path = render_device.lc_ctx().runtime_directory().parent_path() /
                       (luisa::string("shader_build_") + backend);
    auto &cmdlist = render_device.lc_main_cmd_list();

    // Initialize scene manager
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

    // Load shaders
    {
        luisa::fiber::counter counter;
        sm->load_shader(counter);
        warm_up_accel();
        counter.wait();
    }

    // Initialize render plugin
    auto render_module = DynamicModule::load("rbc_render_plugin");
    auto render_plugin = RBC_LOAD_PLUGIN(render_module, RenderPlugin);
    StateMap pipeline_state_map;
    auto pipe_ctx = render_plugin->create_pipeline_context(pipeline_state_map);
    render_plugin->update_skybox("../sky.bytes", uint2(4096, 2048));
    LUISA_ASSERT(render_plugin->initialize_pipeline({}));

    // Initialize window
    Window window("RoboCute Editor", resolution);
    auto swapchain = render_device.lc_device().create_swapchain(
        present_stream,
        SwapchainOption{
            .display = window.native_display(),
            .window = window.native_handle(),
            .size = resolution,
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 2});

    // Initialize editor scene
    vstd::optional<EditorScene> editor_scene;
    editor_scene.create();

    // Render loop state
    auto timeline_event = render_device.lc_device().create_timeline_event();
    auto stream_evt = render_device.lc_device().create_event();
    uint64_t render_frame_index = 0;
    uint64_t frame_index = 0;
    auto dst_img = render_device.lc_device().create_image<float>(
        swapchain.backend_storage(), resolution);

    Clock clk;
    double last_frame_time = 0;
    double last_sync_time = 0;
    const double sync_interval = 0.1;// Sync every 100ms

    // Camera controls
    vstd::optional<float3> camera_move;
    window.set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        if (action != Action::ACTION_PRESSED) return;
        render_frame_index = 0;
        switch (key) {
            case Key::KEY_SPACE:
                LUISA_INFO("Reset frame");
                break;
            case Key::KEY_W:
                camera_move.create();
                *camera_move += float3(0, 0, -0.1);
                break;
            case Key::KEY_S:
                camera_move.create();
                *camera_move += float3(0, 0, 0.1);
                break;
            case Key::KEY_A:
                camera_move.create();
                *camera_move += float3(-0.1, 0, 0);
                break;
            case Key::KEY_D:
                camera_move.create();
                *camera_move += float3(0.1, 0, 0);
                break;
            case Key::KEY_Q:
                camera_move.create();
                *camera_move += float3(0, -0.1, 0);
                break;
            case Key::KEY_E:
                camera_move.create();
                *camera_move += float3(0, 0.1, 0);
                break;
        }
    });

    // Set camera FOV
    render_plugin->get_camera(pipe_ctx).fov = radians(80.f);

    LUISA_INFO("Editor ready, entering render loop...");
    LUISA_INFO("Controls: WASD+QE to move camera, Space to reset frame");

    // Main render loop
    while (!window.should_close()) {
        AssetsManager::instance()->wake_load_thread();
        window.poll_events();

        // Wait for previous frames
        if (frame_index > 2) {
            timeline_event.synchronize(frame_index - 2);
        }

        // Sync with server periodically
        auto time = clk.toc();
        if (time - last_sync_time > sync_interval) {
            if (scene_sync.update_from_server(http_client)) {
                LUISA_INFO("Scene updated from server");
                editor_scene->update_from_sync(scene_sync);
                render_frame_index = 0;// Reset accumulation
            }
            last_sync_time = time;

            // Send heartbeat
            http_client.send_heartbeat(editor_id);
        }

        // Setup frame settings
        auto &frame_settings = pipeline_state_map.read_mut<rbc::FrameSettings>();
        frame_settings.render_resolution = dst_img.size();
        frame_settings.display_resolution = dst_img.size();
        frame_settings.dst_img = &dst_img;

        auto delta_time = time - last_frame_time;
        last_frame_time = time;
        frame_settings.delta_time = (float)delta_time;
        frame_settings.time = time;
        frame_settings.frame_index = render_frame_index;
        ++render_frame_index;

        // Handle camera movement
        if (camera_move) {
            auto &camera = render_plugin->get_camera(pipe_ctx);
            // Simple camera position update
            // TODO: Implement proper camera controller
            *camera_move = float3(0);
            camera_move.destroy();
            render_frame_index = 0;
        }

        // Wait for display
        compute_stream << stream_evt.wait();

        // Render frame
        before_frame(render_plugin, pipe_ctx);
        after_frame(render_plugin, pipe_ctx, &timeline_event, ++frame_index);

        // Present
        present_stream << timeline_event.wait(frame_index)
                       << swapchain.present(dst_img)
                       << stream_evt.signal();
    }

    // Cleanup
    LUISA_INFO("Shutting down editor...");

    timeline_event.synchronize(frame_index);
    present_stream.synchronize();
    stream_evt.synchronize();

    if (!cmdlist.empty()) {
        compute_stream << cmdlist.commit();
    }

    auto pipe_settings_json = pipeline_state_map.serialize_to_json();
    if (pipe_settings_json.data()) {
        LUISA_INFO("Pipeline settings: {}",
                   luisa::string_view{
                       (char const *)pipe_settings_json.data(),
                       pipe_settings_json.size()});
    }

    compute_stream.synchronize();

    // Destroy resources
    editor_scene.destroy();
    render_plugin->destroy_pipeline_context(pipe_ctx);
    render_plugin->dispose();

    AssetsManager::destroy_instance();
    sm->before_rendering(cmdlist, compute_stream);
    sm->on_frame_end(cmdlist, compute_stream);
    compute_stream.synchronize();
    sm.destroy();

    LUISA_INFO("Editor shutdown complete");
    return 0;
}

// Helper functions from test_graphics
void warm_up_accel() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &render_device = rbc::RenderDevice::instance();

    // Build a simple accel to preload driver builtin shaders
    auto buffer = render_device.lc_device().create_buffer<uint>(4 * 3 + 3);
    auto vb = buffer.view(0, 4 * 3).as<float3>();
    auto ib = buffer.view(4 * 3, 3).as<Triangle>();
    uint tri[] = {0, 1, 2};
    float data[] = {
        0, 0, 0, 0,
        0, 1, 0, 0,
        1, 1, 0, 0,
        reinterpret_cast<float &>(tri[0]),
        reinterpret_cast<float &>(tri[1]),
        reinterpret_cast<float &>(tri[2])};
    auto mesh = render_device.lc_device().create_mesh(vb, ib, AccelOption{.hint = AccelUsageHint::FAST_BUILD, .allow_compaction = false});
    auto accel = render_device.lc_device().create_accel(AccelOption{.hint = AccelUsageHint::FAST_BUILD, .allow_compaction = false});
    accel.emplace_back(mesh);
    render_device.lc_main_stream()
        << buffer.view().copy_from(data)
        << mesh.build()
        << accel.build()
        << [accel = std::move(accel), mesh = std::move(mesh), buffer = std::move(buffer)]() {};
}

void before_frame(rbc::RenderPlugin *render_plugin, rbc::RenderPlugin::PipeCtxStub *pipe_ctx) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    render_device.render_loop_mtx().lock();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &main_stream = render_device.lc_main_stream();
    render_plugin->before_rendering({}, pipe_ctx);
    sm.before_rendering(cmdlist, main_stream);
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    managed_device->begin_managing(cmdlist);
}

void after_frame(
    rbc::RenderPlugin *render_plugin,
    rbc::RenderPlugin::PipeCtxStub *pipe_ctx,
    luisa::compute::TimelineEvent *timeline_event,
    uint64_t signal_fence) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    auto &main_stream = render_device.lc_main_stream();
    auto &cmdlist = render_device.lc_main_cmd_list();
    render_plugin->on_rendering({}, pipe_ctx);
    managed_device->end_managing(cmdlist);
    if (timeline_event && signal_fence > 1) {
        timeline_event->synchronize(signal_fence - 1);
    }
    sm.on_frame_end(cmdlist, main_stream, managed_device);
    if (timeline_event && signal_fence > 0) [[likely]] {
        main_stream << timeline_event->signal(signal_fence);
    }
    render_device.render_loop_mtx().unlock();
}
