#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_runtime/render_plugin.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/triangle.h>
#include "utils.h"
namespace rbc {
GraphicsUtils::GraphicsUtils() {}
GraphicsUtils::~GraphicsUtils() {};
void GraphicsUtils::dispose(vstd::function<void()> after_sync) {
    compute_event.event.synchronize(compute_event.fence_index);
    if (display_pipe_ctx)
        render_plugin->destroy_pipeline_context(display_pipe_ctx);
    if (after_sync)
        after_sync();
    render_plugin->dispose();
    AssetsManager::destroy_instance();
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), false);
    sm.destroy();
    present_event.event.synchronize(present_event.fence_index);
    present_stream.synchronize();
    dst_image.reset();
    window.reset();
}
void GraphicsUtils::init_device(luisa::string_view program_path, luisa::string_view backend_name) {
    render_device.init(program_path, backend_name);
    auto &compute_stream = render_device.lc_async_stream();
    render_device.set_main_stream(&compute_stream);
    compute_event.event = render_device.lc_device().create_timeline_event();
    this->backend_name = backend_name;
}
void GraphicsUtils::init_graphics(luisa::filesystem::path const &shader_path) {
    auto &cmdlist = render_device.lc_main_cmd_list();
    sm.create(
        render_device.lc_ctx(),
        render_device.lc_device(),
        render_device.lc_async_copy_stream(),
        *render_device.io_service(),
        cmdlist,
        shader_path);
    AssetsManager::init_instance(render_device, sm);
    // init
    {
        luisa::fiber::counter init_counter;
        sm->load_shader(init_counter);
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
        render_device.lc_main_stream().synchronize();
        init_counter.wait();
    }
}
void GraphicsUtils::init_render() {
    render_module = DynamicModule::load("rbc_render_plugin");
    render_plugin = RBC_LOAD_PLUGIN(render_module, RenderPlugin);
    display_pipe_ctx = render_plugin->create_pipeline_context(render_settings);
    LUISA_ASSERT(render_plugin->initialize_pipeline({}));
}

void GraphicsUtils::init_display(luisa::string_view name, uint2 resolution, bool resizable) {
    auto &device = render_device.lc_device();
    present_stream = device.create_stream(StreamTag::GRAPHICS);
    present_event.event = device.create_timeline_event();
    window.create(luisa::string{name}.c_str(), resolution, resizable);
    swapchain = device.create_swapchain(
        present_stream,
        SwapchainOption{
            .display = window->native_display(),
            .window = window->native_handle(),
            .size = resolution,
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 1});
    dst_image = render_device.lc_device().create_image<float>(swapchain.backend_storage(), resolution);
}
void GraphicsUtils::reset_frame() {
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), true);
    render_plugin->clear_context(display_pipe_ctx);
}

bool GraphicsUtils::should_close() {
    return window && window->should_close();
}
void GraphicsUtils::tick(vstd::function<void()> before_render) {
    sm->prepare_frame();
    AssetsManager::instance()->wake_load_thread();
    if (window)
        window->poll_events();
    if (require_reset) {
        require_reset = false;
        reset_frame();
    }

    // TODO: later for many context
    if (!display_pipe_ctx)
        return;
    if (before_render) {
        before_render();
    }
    auto &main_stream = render_device.lc_main_stream();
    if (present_event.fence_index > 0)
        main_stream << present_event.event.wait(present_event.fence_index);
    render_device.render_loop_mtx().lock();
    auto &cmdlist = render_device.lc_main_cmd_list();
    render_plugin->before_rendering({}, display_pipe_ctx);
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    managed_device->begin_managing(cmdlist);
    sm->before_rendering(
        cmdlist,
        main_stream);
    // on render
    render_plugin->on_rendering({}, display_pipe_ctx);
    // TODO: pipeline update
    //////////////// Test
    managed_device->end_managing(cmdlist);
    sm->on_frame_end(
        cmdlist,
        main_stream, managed_device);
    main_stream << compute_event.event.signal(++compute_event.fence_index);
    auto prepare_next_frame = vstd::scope_exit([&] {
        if (compute_event.fence_index > 2)
            compute_event.event.synchronize(compute_event.fence_index - 2);
        sm->prepare_frame();
    });
    render_device.render_loop_mtx().unlock();
    /////////// Present
    present_stream << compute_event.event.wait(compute_event.fence_index);
    if (present_event.fence_index > 1) {
        present_event.event.synchronize(present_event.fence_index - 1);
    }
    present_stream << swapchain.present(dst_image);
    // for (auto &i : rpc_hook.shared_window.swapchains) {
    //     present_stream << i.second.present(dst_img);
    // }
    present_stream << present_event.event.signal(++present_event.fence_index);
}
void GraphicsUtils::resize_swapchain(uint2 size) {
    reset_frame();
    present_event.event.synchronize(present_event.fence_index);
    present_stream.synchronize();
    dst_image.reset();
    dst_image = render_device.lc_device().create_image<float>(swapchain.backend_storage(), size);
    swapchain.reset();
    swapchain = render_device.lc_device().create_swapchain(
        present_stream,
        SwapchainOption{
            .display = window->native_display(),
            .window = window->native_handle(),
            .size = size,
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 1});
}
}// namespace rbc