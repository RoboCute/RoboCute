#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/device_assets/device_buffer.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_transforming_mesh.h>
#include <rbc_render/render_plugin.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_world/base_object.h>
#include <rbc_world/component.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_core/state_map.h>
#include <rbc_graphics/compute_device.h>
#include <rbc_graphics/lights.h>
#include <luisa/backends/ext/cuda_external_ext.h>
#include <luisa/backends/ext/dx_cuda_interop.h>
#include <luisa/backends/ext/vk_cuda_interop.h>

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
namespace rbc {
#include <material/mats.inl>

static GraphicsUtils *_graphics_utils_singleton{};
GraphicsUtils *GraphicsUtils::instance() {
    return _graphics_utils_singleton;
}
GraphicsUtils::GraphicsUtils() {
}
GraphicsUtils::~GraphicsUtils() {
    dispose();
}

void deser_openpbr(
    [[maybe_unused]] JsonSerializer &serde,
    [[maybe_unused]] material::OpenPBR &x) {
}
void GraphicsUtils::dispose(vstd::function<void()> after_sync) {
    if (!_graphics_utils_singleton) return;
    if (_tex_loader) {
        _tex_loader->finish_task();
        _tex_loader.reset();
    }
    LUISA_ASSERT(_graphics_utils_singleton == this);
    _graphics_utils_singleton = nullptr;
    if (_sm) {
        _sm->refresh_pipeline(_render_device->lc_main_cmd_list(), _render_device->lc_main_stream(), false, true);
    }
    if (_compute_event.event)
        _compute_event.event.synchronize(_compute_event.fence_index);
    if (after_sync)
        after_sync();
    for (auto &i : _render_pipe_ctxs) {
        _render_plugin->destroy_pipeline_context(i);
    }
    _denoise_packs.clear();
    _render_module.reset();
    if (_lights)
        _lights.reset();
    AssetsManager::destroy_instance();
    if (_sm) {
        _sm->tex_streamer().dispose();
    }
    RenderDevice::instance()._dispose_io_service();
    if (_sm) {
        _render_device->execute_before_cmdlist_commit_task();
        _sm->mesh_manager().on_frame_end(nullptr, _sm->bindless_allocator());
        _render_device->execute_after_cmdlist_commit_task();
        _sm.reset();
    }
    if (_present_stream) {
        _present_stream.synchronize();
    }
    _dst_image = {};
    _present_image = {};
}
void GraphicsUtils::init_device(luisa::string_view program_path, luisa::string_view backend_name, bool compactible_mode) {
    LUISA_ASSERT(!_graphics_utils_singleton);
    _graphics_utils_singleton = this;
    PluginManager::init();
    Context ctx{program_path};
    _render_device = vstd::make_unique<RenderDevice>(compactible_mode);
    _render_device->init(Context{ctx}, backend_name);
    if (RenderDevice::instance().get_features().cuda_device) {
        _compute_device = vstd::make_unique<ComputeDevice>();
        _compute_device->init(Context{ctx}, Device{_render_device->lc_device()});
    }
    _backend_name = backend_name;
}
void GraphicsUtils::init_graphics(luisa::filesystem::path const &shader_path) {
    auto &cmdlist = _render_device->lc_main_cmd_list();
    _sm = vstd::make_unique<SceneManager>(
        _render_device->lc_ctx(),
        _render_device->lc_device(),
        _render_device->lc_async_copy_stream(),
        *_render_device->io_service(),
        cmdlist,
        shader_path);
    AssetsManager::init_instance(*_render_device, _sm.get());
    init_present_stream();
    luisa::fiber::counter init_counter;
    _render_device->set_main_stream(&_present_stream);
    _compute_event.event = _render_device->lc_device().create_timeline_event();
    _sm->load_shader(init_counter);
    (void)_sm->mat_manager().emplace_mat_type<material::PolymorphicMaterial, material::OpenPBR>(
        _sm->bindless_allocator(),
        65536);
    (void)_sm->mat_manager().emplace_mat_type<material::PolymorphicMaterial, material::Unlit>(
        _sm->bindless_allocator(),
        65536);
    _tex_loader = luisa::make_unique<TextureLoader>();
    init_counter.wait();
    _lights = vstd::make_unique<Lights>();
}
StateMap &GraphicsUtils::render_settings(RenderPlugin::PipeCtxStub *pipe_ctx) const {
    return *_render_plugin->pipe_ctx_state_map(pipe_ctx);
}
RenderPlugin::PipeCtxStub *GraphicsUtils::register_render_pipectx() {
    if (!_render_plugin) [[unlikely]] {
        LUISA_ERROR("Render plugin not initialized.");
    }
    auto pipe_ctx = _render_plugin->create_pipeline_context();
    _render_pipe_ctxs.emplace(pipe_ctx);
    return pipe_ctx;
}
void GraphicsUtils::init_render() {
    _render_module = PluginManager::instance().load_module("rbc_render_plugin");
    LUISA_ASSERT(_render_module, "Render module not found.");
    _render_plugin = _render_module->invoke<RenderPlugin *()>("get_render_plugin");
    LUISA_ASSERT(_render_plugin->initialize_pipeline({}));
    _sm->refresh_pipeline(_render_device->lc_main_cmd_list(), _render_device->lc_main_stream(), false, false);
    _sm->prepare_frame();
    _denoiser_inited = _render_plugin->init_oidn();
    _render_plugin->sync_init();
}

void GraphicsUtils::init_present_stream() {
    auto &device = _render_device->lc_device();
    if (!_present_stream)
        _present_stream = device.create_stream(StreamTag::GRAPHICS);
}

void GraphicsUtils::init_display(
    uint2 resolution,
    uint64_t native_display,
    uint64_t native_handle,
    bool transparent) {
    init_present_stream();
    if (!_dst_image || any(_dst_image.size() != resolution) ||
        !_present_image || any(_present_image.size() != resolution)) {
        resize_swapchain(resolution, native_display, native_handle, transparent);
    }
}

void GraphicsUtils::reset_frame() {
    _sm->refresh_pipeline(_render_device->lc_main_cmd_list(), _render_device->lc_main_stream(), true, true);
    for (auto &i : _render_pipe_ctxs) {
        _render_plugin->clear_context(i);
    }
}

void GraphicsUtils::remove_render_pipectx(RenderPlugin::PipeCtxStub *pipe_ctx) {
    if (!_render_plugin) [[unlikely]] {
        LUISA_ERROR("Render plugin not initialized.");
    }
    _render_pipe_ctxs.remove(pipe_ctx);
    _frame_requires_sync = true;
    _render_plugin->destroy_pipeline_context(pipe_ctx);
}

void GraphicsUtils::tick(
    TickStage tick_stage,
    bool enable_denoise) {
    if (_tex_loader) {
        _tex_loader->finish_task();
    }
    world::Component::_zz_invoke_world_event(world::WorldEventType::BeforeFrame);
    AssetsManager::instance()->wake_load_thread();
    std::unique_lock render_lck{_render_device->render_loop_mtx()};
    if (_frame_requires_sync.exchange(false)) {
        _compute_event.event.synchronize(_compute_event.fence_index);
    }
    _sm->prepare_frame();
    if (_require_reset) {
        _require_reset = false;
        reset_frame();
    }

    // TODO: later for many context
    auto &cmdlist = _render_device->lc_main_cmd_list();
    auto &main_stream = _render_device->lc_main_stream();
    auto dispose_denoise_pack = vstd::scope_exit([&] {
        for (auto &i : _denoise_packs) {
            if (i.external_albedo)
                _sm->dispose_after_sync(std::move(i.external_albedo));
            if (i.external_input)
                _sm->dispose_after_sync(std::move(i.external_input));
            if (i.external_output)
                _sm->dispose_after_sync(std::move(i.external_output));
            if (i.external_normal)
                _sm->dispose_after_sync(std::move(i.external_normal));
        }
    });
    _denoise_packs.clear();
    _denoise_packs.reserve(_render_pipe_ctxs.size());
    auto init_render_ctx = [&](RenderPlugin::PipeCtxStub *pipe_ctx) {
        auto render_settings = _render_plugin->pipe_ctx_state_map(pipe_ctx);
        auto const &render_view = render_settings->read<RenderView>();
        auto &frame_settings = render_settings->read_mut<rbc::FrameSettings>();
        auto &pipe_settings = render_settings->read_mut<rbc::PTPipelineSettings>();
        // TODO: camera settings
        frame_settings.resolved_img = {};
        if (tick_stage == TickStage::None) {
            pipe_settings.render = false;
            return;
        }
        auto img = render_view.img ? render_view.img : &_dst_image;
        frame_settings.display_offset = min(render_view.view_offset_pixels, img->size() - 1u);
        frame_settings.display_resolution = min(render_view.view_size_pixels, img->size() - frame_settings.display_offset);
        frame_settings.render_resolution = frame_settings.display_resolution;// desired for super-sampling
        frame_settings.dst_img = img;
        frame_settings.id_img = render_view.id_img;
        frame_settings.albedo_buffer = nullptr;
        frame_settings.normal_buffer = nullptr;
        frame_settings.radiance_buffer = nullptr;
        auto pt_settings = render_settings->read_if<PathTracerSettings>();
        enable_denoise &= _denoiser_inited && (!pt_settings || pt_settings->denoise);
        DenoisePack *denoise_pack{};
        if (tick_stage != TickStage::RasterPreview && enable_denoise) {
            denoise_pack = &_denoise_packs.emplace_back();
            *denoise_pack = _render_plugin->create_denoise_task(
                main_stream,
                pipe_ctx,
                frame_settings.display_resolution);
        }

        pipe_settings.render = true;
        auto set_denoise_pack = [&]() {
            if (enable_denoise) {
                frame_settings.albedo_buffer = &denoise_pack->external_albedo;
                frame_settings.normal_buffer = &denoise_pack->external_normal;
                frame_settings.radiance_buffer = &denoise_pack->external_input;
                // frame_settings.reject_sampling = true;
            }
        };
        switch (tick_stage) {
            case TickStage::None:
                break;
            case TickStage::RasterPreview:
                pipe_settings.use_raster = true;
                pipe_settings.use_raytracing = false;
                pipe_settings.use_editing = true;
                pipe_settings.use_post_filter = true;
                break;
            case TickStage::PathTracingPreview:
                set_denoise_pack();
                pipe_settings.use_raster = false;
                pipe_settings.use_raytracing = true;
                pipe_settings.use_editing = true;
                pipe_settings.use_post_filter = true;
                break;
            case TickStage::OffineCapturing:
                set_denoise_pack();
                pipe_settings.use_raster = false;
                pipe_settings.use_raytracing = true;
                pipe_settings.use_post_filter = false;
                pipe_settings.use_editing = false;
                break;
            case TickStage::PresentOfflineResult:
                if (enable_denoise) {
                    frame_settings.radiance_buffer = &denoise_pack->external_output;
                }
                pipe_settings.use_raster = false;
                pipe_settings.use_raytracing = false;
                pipe_settings.use_post_filter = true;
                pipe_settings.use_editing = false;
                break;
        }
    };
    for (auto &i : _render_pipe_ctxs) {
        init_render_ctx(i);
    }
    rbc::world::_zz_on_before_rendering();
    world::Component::_zz_invoke_world_event(world::WorldEventType::BeforeRender);
    // mesh light accel update
    {
        auto io_cmdlist_last_size = _sm->frame_mem_io_list().commands().size();
        _lights->mesh_light_accel.update_frame(_sm->frame_mem_io_list());
        if (_sm->frame_mem_io_list().commands().size() - io_cmdlist_last_size > 0) {
            _sm->set_io_cmdlist_require_sync();
        }
    }
    _sm->execute_io(_render_device->fallback_mem_io_service(), main_stream, _compute_event.event.handle(), _compute_event.fence_index);
    for (auto &i : _render_pipe_ctxs) {
        _render_plugin->before_rendering({}, i);
    }
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    managed_device->begin_managing(cmdlist);
    _sm->before_rendering(
        cmdlist,
        main_stream);
    // on render
    uint64_t idx = 0;
    _render_device->_pipeline_name_ = {};
    for (auto &i : _render_pipe_ctxs) {
        _render_device->_pipeline_name_ = luisa::format("{}V", idx);
        ++idx;
        _render_plugin->on_rendering({}, i);
    }
    _render_device->_pipeline_name_ = {};
    // TODO: pipeline update
    //////////////// Test
    managed_device->end_managing(cmdlist);
    // blit
    if (_swapchain) {
        _sm->tex_uploader().blit(cmdlist, _dst_image, _present_image, float2(1), float2(), uint2(), _present_image.size());
    }
    AssetsManager::instance()->execute_render_thread();
    _render_device->execute_before_cmdlist_commit_task();
    _sm->on_frame_end(
        cmdlist,
        main_stream, managed_device);
    _render_device->execute_after_cmdlist_commit_task();
    main_stream << _compute_event.event.signal(++_compute_event.fence_index);
    if (_compute_event.fence_index > 2)
        _compute_event.event.synchronize(_compute_event.fence_index - 2);
    _sm->prepare_frame();
    /////////// Present
    if (_swapchain) {
        _present_stream << _swapchain.present(_present_image);
    }
    render_lck.unlock();
    world::Component::_zz_invoke_world_event(world::WorldEventType::AfterFrame);
    // for (auto &i : rpc_hook.shared_window.swapchains) {
    //     _present_stream << i.second.present(dst_img);
    // }
}
void GraphicsUtils::resize_swapchain(
    uint2 size,
    uint64_t native_display,
    uint64_t native_handle,
    bool transparent) {
    if (any(size == 0u)) return;
    reset_frame();
    _frame_requires_sync = false;
    _compute_event.event.synchronize(_compute_event.fence_index);
    _present_stream.synchronize();

    _dst_image = {};
    _swapchain = {};
    _present_image = {};
    _dst_image = _render_device->lc_device().create_image<float>(PixelStorage::FLOAT4, size, 1, false, true);
    _dst_image.set_name("Dest image");

    if (native_handle != invalid_resource_handle) {
        _swapchain = _render_device->lc_device().create_swapchain(
            _present_stream,
            SwapchainOption{
                .display = native_display,
                .window = native_handle,
                .size = size,
                .wants_hdr = false,
                .wants_vsync = false,
                .wants_transparent = transparent,
                .back_buffer_count = 2});
        _present_image = {};
        _present_image = _render_device->lc_device().create_image<float>(_swapchain.backend_storage(), size, 1, false, true);
        _present_image.set_name("Present image");
    }
}
void GraphicsUtils::update_mesh_data(DeviceMesh *mesh, bool only_vertex) {
    mesh->wait_finished();
    if (!mesh->mesh_data()) {
        LUISA_WARNING("Mesh not initialized, can not update data.");
        return;
    }
    mesh->calculate_bounding_box();
    auto mesh_data = mesh->mesh_data();
    LUISA_ASSERT(mesh_data, "Mesh not loaded.");
    auto host_data = mesh->host_data();
    if (host_data.size_bytes() != mesh_data->pack.data.size_bytes()) {
        LUISA_ERROR("Invalid host data length.");
    }

    if (only_vertex) {
        _sm->frame_mem_io_list() << IOCommand{
            host_data.data(),
            0,
            IOBufferSubView{mesh_data->pack.data.view(0, mesh_data->meta.tri_byte_offset / sizeof(uint))}};
    } else {
        _sm->frame_mem_io_list() << IOCommand{
            host_data.data(),
            0,
            IOBufferSubView{mesh_data->pack.data}};
    }
    if (mesh_data->pack.mesh) {
        _sm->set_io_cmdlist_require_sync();
        build_mesh(mesh);
    }
    auto &sm = SceneManager::instance();
    sm.dispose_after_sync(RC<DeviceMesh>(mesh));
}
void GraphicsUtils::create_mesh(
    DeviceMesh *ptr,
    uint32_t vertex_count, bool contained_normal, bool contained_tangent,
    uint32_t uv_count, uint32_t triangle_count, vstd::vector<uint> &&offsets) {
    if (ptr->mesh_data()) {
        LUISA_WARNING("Mesh already initialized, can not inited twice.");
        return;
    }
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
void GraphicsUtils::update_buffer(
    DeviceBuffer *buffer,
    uint64_t offset_bytes,
    uint64_t max_size_bytes) {
    buffer->wait_finished();
    if (buffer->loaded()) {
        _sm->set_io_cmdlist_require_sync();
    }

    buffer->wait_finished();
    auto host_data = buffer->host_data();
    if (host_data.empty()) {
        LUISA_WARNING("Buffer has no host data, can not update.");
        return;
    }

    // Calculate actual size to copy
    uint64_t copy_size = host_data.size_bytes();
    if (offset_bytes >= copy_size) {
        LUISA_ERROR("Buffer offset {} exceeds host data size {}.", offset_bytes, copy_size);
        return;
    }
    copy_size -= offset_bytes;
    if (max_size_bytes < copy_size) {
        copy_size = max_size_bytes;
    }

    // Get device buffer and calculate element offsets (buffer is Buffer<uint>, 4 bytes per element)
    auto &device_buffer = buffer->buffer();
    if (!device_buffer) [[unlikely]] {
        LUISA_ERROR("Device buffer is null, cannot update buffer.");
        return;
    }
    uint64_t element_offset = offset_bytes / sizeof(uint);
    uint64_t element_count = (copy_size + sizeof(uint) - 1) / sizeof(uint);
    element_count = std::min(element_count, device_buffer.size() - element_offset);

    if (element_count == 0) {
        return;
    }

    // Create buffer view and IO command
    auto buffer_view = device_buffer.view(element_offset, element_count);
    _sm->frame_mem_io_list() << IOCommand{
        host_data.data() + offset_bytes,
        0,
        IOBufferSubView{buffer_view}};

    auto &sm = SceneManager::instance();
    sm.dispose_after_sync(RC<DeviceBuffer>(buffer));
}

void GraphicsUtils::update_texture(DeviceImage *ptr, uint mip_level) {
    ptr->wait_finished();
    if (ptr->heap_idx() != ~0u) {
        _sm->set_io_cmdlist_require_sync();
    }
    auto &&img = ptr->get_float_image();
    auto host_ptr = ptr->host_data().data();
    auto copy_view = [&](ImageView<float> img) {
        auto size_bytes = img.size_bytes();
        auto row_size = pixel_storage_size(img.storage(), make_uint3(img.size().x, 1, 1));
        if ((row_size & 255) > 0) {
            _render_device->lc_main_cmd_list() << img.copy_from(luisa::span(host_ptr, size_bytes));
        } else {
            _sm->frame_mem_io_list() << IOCommand{
                host_ptr,
                0,
                IOTextureSubView{img}};
        }
        host_ptr += size_bytes;
    };
    if (mip_level == ~0u) {
        for (auto i : vstd::range(img.mip_levels())) {
            copy_view(img.view(i));
        }
    } else {
        mip_level = std::min<uint32_t>(img.mip_levels() - 1, mip_level);
        for (auto i : vstd::range(mip_level)) {
            host_ptr += img.view(i).size_bytes();
        }
        copy_view(img.view(mip_level));
    }

    auto &sm = SceneManager::instance();
    sm.dispose_after_sync(RC<DeviceImage>(ptr));
}
bool GraphicsUtils::denoise() {
    if (!_denoiser_inited) return false;
    if (_denoise_packs.empty()) {
        LUISA_WARNING("No denoised frame ready.");
        return false;
    }
    for (auto &i : _denoise_packs) {
        if (!i.denoise_callback)
            LUISA_ERROR("Denoiser not ready.");
        i.denoise_callback();
    }
    return true;
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
void GraphicsUtils::build_mesh(DeviceMesh *mesh) {
    auto &blas = mesh->mesh_data()->pack.mesh;
    if (blas)
        _sm->build_mesh_in_frame(&blas, RC<RCBase>{mesh});
}
void GraphicsUtils::build_transforming_mesh(DeviceTransformingMesh *mesh) {
    auto &blas = mesh->mesh_data()->pack.mesh;
    if (blas)
        _sm->build_mesh_in_frame(&blas, RC<RCBase>{mesh});
}
void GraphicsUtils::update_skinning(
    world::MeshResource *skinning_mesh,
    BufferView<DualQuaternion> bones) {
    auto origin_mesh = skinning_mesh->origin_mesh();
    auto origin_mesh_data = origin_mesh->mesh_data();

    auto device_mesh = skinning_mesh->device_transforming_mesh();
    if (!(origin_mesh && device_mesh)) {
        if (!skinning_mesh->is_transforming_mesh()) [[unlikely]] {
            LUISA_ERROR("Updating to non-skinning mesh.");
        } else {
            LUISA_ERROR("Invalid skinning mesh.");
        }
    }
    auto weight_index_buffer = origin_mesh->get_or_create_property_buffer("skinning_weight_index");
    if (!weight_index_buffer) [[unlikely]] {
        LUISA_ERROR("Static mesh must have \"skinning_weight_index\" property.");
    }
    auto weight_count = (weight_index_buffer.size_bytes() / sizeof(uint)) / 2;
    auto vert_count = origin_mesh_data->meta.vertex_count;
    if (weight_count % vert_count > 0) [[unlikely]] {
        LUISA_ERROR("Weight count {} invalid with vertex count {}", weight_count, vert_count);
    }
    auto buffer = weight_index_buffer.as<uint>();
    auto weight_buffer = buffer.subview(0, weight_count).as<float>();
    auto index_buffer = buffer.subview(weight_count, weight_count);
    auto &cmdlist = RenderDevice::instance().lc_main_cmd_list();
    Skinning::instance()->update_mesh(
        cmdlist,
        skinning_mesh->mesh_data(),
        origin_mesh_data,
        bones,
        weight_buffer,
        index_buffer);
    build_transforming_mesh(device_mesh);
}
void GraphicsUtils::set_pipeline_ctx_geometry(
    RenderPlugin::PipeCtxStub *pipe_ctx,
    BufferView<float> buffer,
    GeometryType type) {
    auto &map = render_settings(pipe_ctx);
    auto &s = map.read_mut<FrameSettings>();
    s.geometry_channel = type;
    s.pt_geometry_buffer = buffer;
}

}// namespace rbc