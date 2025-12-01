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
template<typename T, typename OpenPBR>
void serde_openpbr(
    T t,
    OpenPBR x) {
    auto serde_func = [&]<typename U>(U &u, char const *name) {
        using PureU = std::remove_cvref_t<U>;
        constexpr bool is_index = requires { u.index; };
        constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
        if constexpr (rbc::is_serializer_v<std::remove_cvref_t<T>>) {
            if constexpr (is_index) {
                t._store(u.index, name);
            } else if constexpr (is_array) {
                t.start_array();
                for (auto &i : u) {
                    t._store(i);
                }
                t.add_last_scope_to_object(name);
            } else {
                t._store(u, name);
            }
        } else {
            if constexpr (is_index) {
                t._load(u.index, name);
            } else if constexpr (is_array) {
                uint64_t size;
                if (!t.start_array(size, name))
                    return;
                LUISA_ASSERT(size == u.size(), "Variable {} array size mismatch.", name);
                for (auto &i : u) {
                    t._load(i);
                }
                t.end_scope();
            } else {
                t._load(u, name);
            }
        }
    };
    serde_func(x.weight.base, "weight_base");
    serde_func(x.weight.diffuse_roughness, "weight_diffuse_roughness");
    serde_func(x.weight.specular, "weight_specular");
    serde_func(x.weight.metallic, "weight_metallic");
    serde_func(x.weight.metallic_roughness_tex, "weight_metallic_roughness_tex");
    serde_func(x.weight.subsurface, "weight_subsurface");
    serde_func(x.weight.transmission, "weight_transmission");
    serde_func(x.weight.thin_film, "weight_thin_film");
    serde_func(x.weight.fuzz, "weight_fuzz");
    serde_func(x.weight.coat, "weight_coat");
    serde_func(x.weight.diffraction, "weight_diffraction");
    serde_func(x.geometry.cutout_threshold, "geometry_cutout_threshold");
    serde_func(x.geometry.opacity, "geometry_opacity");
    serde_func(x.geometry.opacity_tex, "geometry_opacity_tex");
    serde_func(x.geometry.thickness, "geometry_thickness");
    serde_func(x.geometry.thin_walled, "geometry_thin_walled");
    serde_func(x.geometry.nested_priority, "geometry_nested_priority");
    serde_func(x.geometry.bump_scale, "geometry_bump_scale");
    serde_func(x.geometry.normal_tex, "geometry_normal_tex");
    serde_func(x.uvs.uv_scale, "uv_scale");
    serde_func(x.uvs.uv_offset, "uv_offset");
    serde_func(x.specular.specular_color, "specular_color");
    serde_func(x.specular.roughness, "specular_roughness");
    serde_func(x.specular.roughness_anisotropy, "specular_roughness_anisotropy");
    serde_func(x.specular.specular_anisotropy_level_tex, "specular_anisotropy_level_tex");
    serde_func(x.specular.roughness_anisotropy_angle, "specular_roughness_anisotropy_angle");
    serde_func(x.specular.specular_anisotropy_angle_tex, "specular_anisotropy_angle_tex");
    serde_func(x.specular.ior, "specular_ior");
    serde_func(x.emission.luminance, "emission_luminance");
    serde_func(x.emission.emission_tex, "emission_tex");
    serde_func(x.base.albedo, "base_albedo");
    serde_func(x.base.albedo_tex, "base_albedo_tex");
    serde_func(x.subsurface.subsurface_color, "subsurface_color");
    serde_func(x.subsurface.subsurface_radius, "subsurface_radius");
    serde_func(x.subsurface.subsurface_radius_scale, "subsurface_radius_scale");
    serde_func(x.subsurface.subsurface_scatter_anisotropy, "subsurface_scatter_anisotropy");
    serde_func(x.transmission.transmission_color, "transmission_color");
    serde_func(x.transmission.transmission_depth, "transmission_depth");
    serde_func(x.transmission.transmission_scatter, "transmission_scatter");
    serde_func(x.transmission.transmission_scatter_anisotropy, "transmission_scatter_anisotropy");
    serde_func(x.transmission.transmission_dispersion_scale, "transmission_dispersion_scale");
    serde_func(x.transmission.transmission_dispersion_abbe_number, "transmission_dispersion_abbe_number");
    serde_func(x.coat.coat_color, "coat_color");
    serde_func(x.coat.coat_roughness, "coat_roughness");
    serde_func(x.coat.coat_roughness_anisotropy, "coat_roughness_anisotropy");
    serde_func(x.coat.coat_roughness_anisotropy_angle, "coat_roughness_anisotropy_angle");
    serde_func(x.coat.coat_ior, "coat_ior");
    serde_func(x.coat.coat_darkening, "coat_darkening");
    serde_func(x.coat.coat_roughening, "coat_roughening");
    serde_func(x.fuzz.fuzz_color, "fuzz_color");
    serde_func(x.fuzz.fuzz_roughness, "fuzz_roughness");
    serde_func(x.diffraction.diffraction_color, "diffraction_color");
    serde_func(x.diffraction.diffraction_thickness, "diffraction_thickness");
    serde_func(x.diffraction.diffraction_inv_pitch_x, "diffraction_inv_pitch_x");
    serde_func(x.diffraction.diffraction_inv_pitch_y, "diffraction_inv_pitch_y");
    serde_func(x.diffraction.diffraction_angle, "diffraction_angle");
    serde_func(x.diffraction.diffraction_lobe_count, "diffraction_lobe_count");
    serde_func(x.diffraction.diffraction_type, "diffraction_type");
    serde_func(x.thin_film.thin_film_thickness, "thin_film_thickness");
    serde_func(x.thin_film.thin_film_ior, "thin_film_ior");
}

void GraphicsUtils::openpbr_json_ser(JsonSerializer &json_ser, material::OpenPBR const &mat) {
    serde_openpbr<JsonSerializer &, material::OpenPBR const &>(json_ser, mat);
}
void GraphicsUtils::openpbr_json_deser(JsonDeSerializer &json_deser, material::OpenPBR &mat) {
    serde_openpbr<JsonDeSerializer &, material::OpenPBR &>(json_deser, mat);
}
void GraphicsUtils::openpbr_json_ser(JsonSerializer &json_ser, material::Unlit const &mat) {
    LUISA_NOT_IMPLEMENTED();
}
void GraphicsUtils::openpbr_json_deser(JsonDeSerializer &json_deser, material::Unlit &mat) {
    LUISA_NOT_IMPLEMENTED();
}
void deser_openpbr(
    JsonSerializer &serde,
    material::OpenPBR &x) {
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