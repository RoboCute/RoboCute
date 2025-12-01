#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_runtime/render_plugin.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/triangle.h>
#include "utils.h"
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>
namespace rbc {
GraphicsUtils::GraphicsUtils() {}
GraphicsUtils::~GraphicsUtils() {};
void deser_openpbr(
    JsonSerializer &serde,
    material::OpenPBR &x) {
    // serde._store(x.weight.metallic_roughness_tex, "weight.metallic_roughness_tex");
    // serde._store(x.weight.base, "weight.base");
    // serde._store(x.weight.diffuse_roughness, "weight.diffuse_roughness");
    // serde._store(x.weight.specular, "weight.specular");
    // serde._store(x.weight.metallic, "weight.metallic");
    // serde._store(x.weight.subsurface, "weight.subsurface");
    // serde._store(x.weight.transmission, "weight.transmission");
    // serde._store(x.weight.thin_film, "weight.thin_film");
    // serde._store(x.weight.fuzz, "weight.fuzz");
    // serde._store(x.weight.coat, "weight.coat");
    // serde._store(x.weight.diffraction, "weight.diffraction");
    // serde._store(x.geometry.thickness, "geometry.thickness");
    // serde._store(x.geometry.cutout_threshold, "geometry.cutout_threshold");
    // serde._store(x.geometry.opacity, "geometry.opacity");
    // serde._store(x.geometry.bump_scale, "geometry.bump_scale");
    // serde._store(x.geometry.opacity_tex, "geometry.opacity_tex");
    // serde._store(x.geometry.nested_priority, "geometry.nested_priority");
    // serde._store(x.geometry.normal_tex, "geometry.normal_tex");
    // serde._store(x.geometry.thin_walled, "geometry.thin_walled");
    // serde._store(x.uvs.uv_scale, "uvs.uv_scale");
    // serde._store(x.uvs.uv_offset, "uvs.uv_offset");
    // serde._store(x.specular.specular_color_and_rough, "specular.specular_color_and_rough");
    // serde._store(x.specular.roughness_anisotropy, "specular.roughness_anisotropy");
    // serde._store(x.specular.roughness_anisotropy_angle, "specular.roughness_anisotropy_angle");
    // serde._store(x.specular.ior, "specular.ior");
    // serde._store(x.specular.specular_anisotropy_angle_tex, "specular.specular_anisotropy_angle_tex");
    // serde._store(x.specular.specular_anisotropy_level_tex, "specular.specular_anisotropy_level_tex");
    // serde._store(x.emission.luminance, "emission.luminance");
    // serde._store(x.emission.emission_tex, "emission.emission_tex");
    // serde._store(x.base.albedo, "base.albedo");
    // serde._store(x.base.albedo_tex, "base.albedo_tex");
    // serde._store(x.subsurface.subsurface_color_and_radius, "subsurface.subsurface_color_and_radius");
    // serde._store(x.subsurface.subsurface_radius_scale_andaniso, "subsurface.subsurface_radius_scale_andaniso");
    // serde._store(x.transmission.transmission_color_and_depth, "transmission.transmission_color_and_depth");
    // serde._store(x.transmission.transmission_scatter_and_aniso, "transmission.transmission_scatter_and_aniso");
    // serde._store(x.transmission.transmission_dispersion_scale, "transmission.transmission_dispersion_scale");
    // serde._store(x.transmission.transmission_dispersion_abbe_number, "transmission.transmission_dispersion_abbe_number");
    // serde._store(x.coat.coat_color_and_roughness, "coat.coat_color_and_roughness");
    // serde._store(x.coat.coat_roughness_anisotropy, "coat.coat_roughness_anisotropy");
    // serde._store(x.coat.coat_roughness_anisotropy_angle, "coat.coat_roughness_anisotropy_angle");
    // serde._store(x.coat.coat_ior, "coat.coat_ior");
    // serde._store(x.coat.coat_darkening, "coat.coat_darkening");
    // serde._store(x.coat.coat_roughening, "coat.coat_roughening");
    // serde._store(x.fuzz.fuzz_color_and_roughness, "fuzz.fuzz_color_and_roughness");
    // serde._store(x.diffraction.diffraction_color_and_thickness, "diffraction.diffraction_color_and_thickness");
    // serde._store(x.diffraction.diffraction_inv_pitch_x, "diffraction.diffraction_inv_pitch_x");
    // serde._store(x.diffraction.diffraction_inv_pitch_y, "diffraction.diffraction_inv_pitch_y");
    // serde._store(x.diffraction.diffraction_angle, "diffraction.diffraction_angle");
    // serde._store(x.diffraction.diffraction_lobe_count, "diffraction.diffraction_lobe_count");
    // serde._store(x.diffraction.diffraction_type, "diffraction.diffraction_type");
    // serde._store(x.thin_film.thin_film_thickness, "thin_film.thin_film_thickness");
    // serde._store(x.thin_film.thin_film_ior, "thin_film.thin_film_ior");
}
void GraphicsUtils::dispose(vstd::function<void()> after_sync) {
    if (sm) {
        sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), false, true);
    }
    if (compute_event.event)
        compute_event.event.synchronize(compute_event.fence_index);
    if (display_pipe_ctx)
        render_plugin->destroy_pipeline_context(display_pipe_ctx);
    if (after_sync)
        after_sync();
    if (render_plugin)
        render_plugin->dispose();
    if (lights)
        lights.destroy();
    AssetsManager::destroy_instance();
    if (sm) {
        sm.destroy();
    }
    if (present_event.event)
        present_event.event.synchronize(present_event.fence_index);
    if (present_stream)
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
    lights.create();
}
void GraphicsUtils::init_render() {
    render_module = DynamicModule::load("rbc_render_plugin");
    render_plugin = RBC_LOAD_PLUGIN(render_module, RenderPlugin);
    display_pipe_ctx = render_plugin->create_pipeline_context(render_settings);
    LUISA_ASSERT(render_plugin->initialize_pipeline({}));
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), false, false);
    sm->prepare_frame();
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
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), true, true);
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