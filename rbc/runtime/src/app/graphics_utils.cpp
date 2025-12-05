#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_runtime/render_plugin.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_app/graphics_utils.h>
#include <rbc_runtime/plugin_manager.h>
#include <rbc_render/renderer_data.h>
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
    if (present_stream)
        present_stream.synchronize();
    dst_image.reset();
    window.reset();
}
void GraphicsUtils::init_device(luisa::string_view program_path, luisa::string_view backend_name) {
    PluginManager::init();
    render_device.init(program_path, backend_name);
    init_present_stream();
    render_device.set_main_stream(&present_stream);
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
    PluginManager::init();
    sm->mat_manager().emplace_mat_type<material::PolymorphicMaterial, material::OpenPBR>(
        sm->bindless_allocator(),
        65536);
    sm->mat_manager().emplace_mat_type<material::PolymorphicMaterial, material::Unlit>(
        sm->bindless_allocator(),
        65536);
    render_module = &PluginManager::instance().load_module("rbc_render_plugin");
    LUISA_ASSERT(render_module, "Render module not found.");
    render_plugin = RBC_LOAD_PLUGIN(*render_module, RenderPlugin);
    display_pipe_ctx = render_plugin->create_pipeline_context(render_settings);
    LUISA_ASSERT(render_plugin->initialize_pipeline({}));
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), false, false);
    sm->prepare_frame();
    denoiser_inited = render_plugin->init_oidn();
}

void GraphicsUtils::init_present_stream() {
    auto &device = render_device.lc_device();
    if (!present_stream)
        present_stream = device.create_stream(StreamTag::GRAPHICS);
}

void GraphicsUtils::init_display(uint2 resolution) {
    auto &device = render_device.lc_device();
    init_present_stream();
    if (dst_image && any(dst_image.size() != resolution)) {
        resize_swapchain(resolution);
    } else if (!dst_image) {
        if (!swapchain && window) {
            swapchain = device.create_swapchain(
                present_stream,
                SwapchainOption{
                    .display = window->native_display(),
                    .window = window->native_handle(),
                    .size = resolution,
                    .wants_hdr = false,
                    .wants_vsync = false,
                    .back_buffer_count = 2});
        }
        dst_image = render_device.lc_device().create_image<float>(swapchain ? swapchain.backend_storage() : PixelStorage::BYTE4, resolution, 1, true);
        dst_image.set_name("Dest image");
    }
}

void GraphicsUtils::init_display_with_window(luisa::string_view name, uint2 resolution, bool resizable) {
    window.create(luisa::string{name}.c_str(), resolution, resizable);
    init_display(resolution);
}
void GraphicsUtils::reset_frame() {
    sm->refresh_pipeline(render_device.lc_main_cmd_list(), render_device.lc_main_stream(), true, true);
    render_plugin->clear_context(display_pipe_ctx);
}

bool GraphicsUtils::should_close() {
    return window && window->should_close();
}
void GraphicsUtils::tick(
    float delta_time,
    uint64_t frame_index,
    uint2 resolution,
    TickStage tick_stage) {
    AssetsManager::instance()->wake_load_thread();
    std::unique_lock render_lck{render_device.render_loop_mtx()};
    sm->prepare_frame();
    if (require_reset) {
        require_reset = false;
        reset_frame();
    }

    // TODO: later for many context
    if (!display_pipe_ctx)
        return;

    auto &frame_settings = render_settings.read_mut<rbc::FrameSettings>();
    auto &pipe_settings = render_settings.read_mut<rbc::PTPipelineSettings>();
    frame_settings.render_resolution = resolution;
    frame_settings.display_resolution = dst_image.size();
    frame_settings.dst_img = &dst_image;
    frame_settings.delta_time = (float)delta_time;
    frame_settings.frame_index = frame_index;
    frame_settings.albedo_buffer = nullptr;
    frame_settings.normal_buffer = nullptr;
    frame_settings.radiance_buffer = nullptr;
    frame_settings.resolved_img = nullptr;
    auto &main_stream = render_device.lc_main_stream();
    auto dispose_denoise_pack = vstd::scope_exit([&] {
        if (denoise_pack.external_albedo)
            sm->dispose_after_sync(std::move(denoise_pack.external_albedo));
        if (denoise_pack.external_input)
            sm->dispose_after_sync(std::move(denoise_pack.external_input));
        if (denoise_pack.external_output)
            sm->dispose_after_sync(std::move(denoise_pack.external_output));
        if (denoise_pack.external_normal)
            sm->dispose_after_sync(std::move(denoise_pack.external_normal));
    });
    denoise_pack = {};
    switch (tick_stage) {
        case TickStage::OffineCapturing:
        case TickStage::PresentOfflineResult:
            if (denoiser_inited) {
                denoise_pack = render_plugin->create_denoise_task(
                    main_stream,
                    frame_settings.display_resolution);
            }
            break;
    }
    switch (tick_stage) {
        case TickStage::RasterPreview:
            pipe_settings.use_raster = true;
            pipe_settings.use_raytracing = false;
            pipe_settings.use_post_filter = false;
            break;
        case TickStage::PathTracingPreview:
            pipe_settings.use_raster = false;
            pipe_settings.use_raytracing = true;
            pipe_settings.use_post_filter = true;
            break;
        case TickStage::OffineCapturing:
            if (denoiser_inited) {
                frame_settings.albedo_buffer = &denoise_pack.external_albedo;
                frame_settings.normal_buffer = &denoise_pack.external_normal;
                frame_settings.radiance_buffer = &denoise_pack.external_input;
            }
            pipe_settings.use_raster = false;
            pipe_settings.use_raytracing = true;
            pipe_settings.use_post_filter = true;
            break;
        case TickStage::PresentOfflineResult:
            if (denoiser_inited) {
                frame_settings.radiance_buffer = &denoise_pack.external_output;
            }
            pipe_settings.use_raster = false;
            pipe_settings.use_raytracing = false;
            pipe_settings.use_post_filter = true;
            break;
    }
    auto &cmdlist = render_device.lc_main_cmd_list();
    if (!frame_mem_io_list.empty()) {
        mem_io_fence = render_device.mem_io_service()->execute(std::move(frame_mem_io_list));
    }
    if (!frame_disk_io_list.empty()) {
        disk_io_fence = render_device.io_service()->execute(std::move(frame_disk_io_list));
    }
    // io sync
    auto sync_io = [&](std::atomic_uint64_t &fence, IOService *service) {
        auto io_fence = fence.exchange(0);
        // support direct-storage
        if (io_fence > 0) {
            while (!service->timeline_signaled(io_fence)) {
                std::this_thread::yield();
            }
            main_stream << service->wait(io_fence);
        }
    };
    sync_io(mem_io_fence, render_device.mem_io_service());
    sync_io(disk_io_fence, render_device.io_service());
    for (auto &i : build_meshes) {
        auto &mesh = i->mesh_data()->pack.mesh;
        if (mesh)
            cmdlist << mesh.build();
    }
    build_meshes.clear();

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
    if (compute_event.fence_index > 2)
        compute_event.event.synchronize(compute_event.fence_index - 2);
    sm->prepare_frame();
    /////////// Present
    if (swapchain)
        present_stream << swapchain.present(dst_image);
    render_lck.unlock();
    // for (auto &i : rpc_hook.shared_window.swapchains) {
    //     present_stream << i.second.present(dst_img);
    // }
}
void GraphicsUtils::resize_swapchain(uint2 size) {
    reset_frame();
    compute_event.event.synchronize(compute_event.fence_index);
    present_stream.synchronize();
    dst_image.reset();
    dst_image = render_device.lc_device().create_image<float>(swapchain ? swapchain.backend_storage() : PixelStorage::BYTE4, size, true);
    dst_image.set_name("Dest image");
    swapchain.reset();
    if (window)
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
void GraphicsUtils::update_mesh_data(DeviceMesh *mesh, bool only_vertex) {
    mesh->calculate_bounding_box();
    mesh->wait_finished();
    auto mesh_data = mesh->mesh_data();
    LUISA_ASSERT(mesh_data, "Mesh not loaded.");
    auto host_data = mesh->host_data();
    if (only_vertex) {
        frame_mem_io_list << IOCommand{
            host_data.data(),
            0,
            IOBufferSubView{mesh_data->pack.data.view(0, mesh_data->meta.tri_byte_offset / sizeof(uint))}};
    } else {
        frame_mem_io_list << IOCommand{
            host_data.data(),
            0,
            IOBufferSubView{mesh_data->pack.data}};
    }
    frame_mem_io_list.add_callback([m = RC<DeviceMesh>(mesh)] {});
    auto &sm = SceneManager::instance();
    if (mesh->tlas_ref_count > 0)
        sm.accel_manager().mark_dirty();
    if (mesh_data->pack.mesh) {
        build_meshes.emplace(mesh);
    }
}
void GraphicsUtils::create_mesh(
    DeviceMesh *ptr,
    uint32_t vertex_count, bool contained_normal, bool contained_tangent,
    uint32_t uv_count, uint32_t triangle_count, vstd::vector<uint> &&offsets) {
    auto mesh_size = DeviceMesh::get_mesh_size(vertex_count, contained_normal, contained_tangent, uv_count, triangle_count);
    auto &host_data = ptr->host_data_ref();
    host_data.push_back_uninitialized(mesh_size);
    auto &render_device = RenderDevice::instance();
    ptr->create_mesh(
        render_device.lc_main_cmd_list(),
        vertex_count,
        contained_normal,
        contained_tangent,
        uv_count,
        triangle_count,
        std::move(offsets));
}
void GraphicsUtils::update_texture(DeviceImage *ptr) {
    frame_mem_io_list << IOCommand{
        ptr->host_data().data(),
        0,
        IOTextureSubView{ptr->get_float_image()}};
    frame_mem_io_list.add_callback([m = RC<DeviceImage>(ptr)] {});
}
void GraphicsUtils::denoise() {
    if (!denoiser_inited) return;
    if (!denoise_pack.denoise_callback) {
        LUISA_ERROR("Denoiser not ready.");
    }
    denoise_pack.denoise_callback();
}
void GraphicsUtils::create_texture(
    DeviceImage *ptr,
    PixelStorage storage,
    uint2 size, uint mip_level) {
    size_t size_bytes = 0;
    for (auto i : vstd::range(mip_level))
        size_bytes += pixel_storage_size(storage, make_uint3(size >> (uint)i, 1u));

    auto &host_data = ptr->host_data_ref();
    host_data.push_back_uninitialized(size_bytes);
    auto &device = RenderDevice::instance().lc_device();
    ptr->create_texture<float>(device, storage, size, mip_level);
}
}// namespace rbc