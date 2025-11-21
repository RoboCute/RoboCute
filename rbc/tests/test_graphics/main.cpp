#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/dsl/sugar.h>

int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
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
    auto shader_path = render_device.lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + backend);
    auto &main_stream = render_device.lc_main_stream();
    auto &cmdlist = render_device.lc_main_cmd_list();
    vstd::optional<SceneManager> sm;
    const uint2 resolution{1024};
    sm.create(
        render_device.lc_ctx(),
        render_device.lc_device(),
        shader_path);

    {
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
        main_stream
            << buffer.view().copy_from(data)
            << mesh.build()
            << accel.build()
            << [accel = std::move(accel), mesh = std::move(mesh), buffer = std::move(buffer)]() {};
    }
    sm->load_shader();
    // render loop
    vstd::optional<TexStreamManager> tex_streamer;
    tex_streamer.create(
        render_device.lc_device(),
        render_device.lc_async_copy_stream(),
        *render_device.io_service(),
        render_device.lc_main_cmd_list(),
        sm->bindless_manager());
    // init window
    Window window("test graphics", resolution);
    auto swapchain = render_device.lc_device().create_swapchain(
        main_stream,
        SwapchainOption{
            .display = window.native_display(),
            .window = window.native_handle(),
            .size = resolution,
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 1});
    // render loop
    auto timeline_event = render_device.lc_device().create_timeline_event();
    uint64_t frame_index = 0;
    auto dst_img = render_device.lc_device().create_image<float>(swapchain.backend_storage(), resolution);
    // shader
    auto draw_shader = render_device.lc_device().compile<2>([](ImageVar<float> img) {
        set_block_size(16, 8, 1);
        auto uv = (make_float2(dispatch_id().xy()) + 0.5f) / make_float2(dispatch_size().xy());
        img.write(dispatch_id().xy(), make_float4(uv * 2.5f, 0.5f, 1.f));
    });
    Callable filmic_aces = [](Float3 x) noexcept {
        constexpr float A = 0.22;
        constexpr float B = 0.30;
        constexpr float C = 0.10;
        constexpr float D = 0.20;
        constexpr float E = 0.01;
        constexpr float F = 0.30;
        x = max(x, make_float3(0.f));
        return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
    };
    auto blit_shader = render_device.lc_device().compile<2>([&](ImageVar<float> img, ImageVar<float> dst_img) {
        auto value = img.read(dispatch_id().xy()).xyz();
        value = filmic_aces(value);
        dst_img.write(dispatch_id().xy(), make_float4(value, 1.f));
    });
    while (!window.should_close()) {
        window.poll_events();
        if (frame_index > 1) {
            timeline_event.synchronize(frame_index - 1);
        }
        ++frame_index;
        // before render
        // TODO: pipeline early-update
        tex_streamer->before_rendering(main_stream, sm->temp_upload_buffer(), cmdlist);
        sm->before_rendering(
            cmdlist,
            main_stream);
        // on render
        auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
        managed_device->begin_managing(cmdlist);
        // frame render logic
        // TODO: pipeline update

        //////////////// Test
        auto transient_img = render_device.create_transient_image<float>("my_transient_image", PixelStorage::FLOAT4, resolution);
        cmdlist << draw_shader(transient_img).dispatch(resolution)
                << blit_shader(transient_img, dst_img).dispatch(resolution);
        managed_device->end_managing(cmdlist);
        sm->on_frame_end(
            cmdlist,
            main_stream, managed_device);

        main_stream << swapchain.present(dst_img) << timeline_event.signal(frame_index);
    }
    // Destroy
    if (!cmdlist.empty()) {
        main_stream << cmdlist.commit();
    }
    main_stream.synchronize();
    tex_streamer.destroy();
    sm.destroy();
    render_device.shutdown();
}