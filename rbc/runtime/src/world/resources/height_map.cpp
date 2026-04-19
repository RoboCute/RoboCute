#include <rbc_world/resources/height_map.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_core/utils/thread_waiter.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/logging.h>

namespace rbc::world {

HeightMapResource::HeightMapResource() = default;

HeightMapResource::~HeightMapResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    remove_procedural_instance();
}

bool HeightMapResource::empty() const {
    std::lock_guard lck{_async_mtx};
    return !_height_img || _resolution.x == 0 || _resolution.y == 0;
}

void HeightMapResource::create_empty(uint2 resolution) {
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last height map resource loading.");
    }
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);

    std::lock_guard lck{_async_mtx};
    _height_img.reset();
    _aabb_and_height_bounding.reset();
    _resolution = resolution;

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) {
        LUISA_WARNING("Cannot create height map: render device not available.");
        return;
    }
    auto &device = render_device->lc_device();

    // Create device image for height data
    _height_img = new DeviceImage();
    _height_img->create_texture<float>(device, PixelStorage::FLOAT1, resolution, 1u);

    // Create device buffer for AABB and height bounds
    _aabb_and_height_bounding = new DeviceBuffer();
    uint2 blocks = block_size();
    size_t aabb_size = blocks.x * blocks.y * sizeof(luisa::compute::AABB);
    size_t height_bounds_size = blocks.x * blocks.y * sizeof(float);
    _aabb_and_height_bounding->create_empty(aabb_size + height_bounds_size, DeviceBuffer::FileLoadType::DeviceOnly);

    // Reset HeightMap surface (will be set up during emplace)
    _height_map_surface = rbc::geometry::HeightMap{
        .heightmap_idx = ~0u,
        .mat_buffer_id = ~0u,
        .aabb_buffer_heap_idx = ~0u,
        .height_minmax_buffer_idx = ~0u,
        .height_minmax_buffer_offset_bytes = static_cast<uint32_t>(aabb_size),
        .block_size = blocks,
    };

    _procedural_prim_dirty = true;

    unsafe_set_loaded();
}

void HeightMapResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.value(_resolution, "resolution");
    ser.ar.value(_procedural_instance_id, "procedural_instance_id");
    ser.ar.value(_procedural_prim_dirty, "procedural_prim_dirty");
}

void HeightMapResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::lock_guard lck{_async_mtx};
    uint2 resolution;
    uint32_t proc_inst_id = ~0u;
    bool procedural_prim_dirty = true;

    if (ser.ar.value(resolution, "resolution")) {
        _resolution = resolution;
    }
    if (ser.ar.value(proc_inst_id, "procedural_instance_id")) {
        _procedural_instance_id = proc_inst_id;
    }
    if (ser.ar.value(procedural_prim_dirty, "procedural_prim_dirty")) {
        _procedural_prim_dirty = procedural_prim_dirty;
    }
}

void HeightMapResource::_compute_aabbs(luisa::compute::CommandList &cmdlist) const {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;
    Shader2D<
        Image<float>,  //height_image,
        Buffer<AABB>,  //output_buffer,
        Buffer<float>,//height_min_max_buffer,
        float2,        //xz_axis_min,
        float2         //xz_axis_max
        > const *compute_aabb_shader = nullptr;
    ShaderManager::instance()->load(
        "procedural_prim/height_compute_aabb.bin",
        compute_aabb_shader);
    if (!compute_aabb_shader) [[unlikely]] {
        LUISA_ERROR("Failed to load compute_aabb shader for HeightMapResource.");
        return;
    }
    cmdlist << (*compute_aabb_shader)(
                   _height_img->get_float_image(),
                   aabb_buffer(),
                   height_bounding_buffer(),
                   float2(-0.5),
                   float2(0.5))
                   .dispatch((_resolution + 31u) & (~31u));
}

void HeightMapResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    luisa::compute::ProceduralPrimitive &procedural_prim,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};
    if (procedural_prim.valid() && (!_procedural_prim_dirty))
        return;

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Compute AABBs for height map blocks
    _compute_aabbs(cmdlist);

    // Create procedural primitive (BLAS) with AABBs
    procedural_prim = device.create_procedural_primitive(
        aabb_buffer(),
        luisa::compute::AccelOption{.allow_compaction = false});

    // Build the procedural primitive
    cmdlist << procedural_prim.build();

    _procedural_prim_dirty = false;
}

uint HeightMapResource::emplace_procedural_instance(
    luisa::float4x4 const &transform,
    uint8_t visibility_mask) {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    luisa::compute::CommandList &cmdlist = RenderDevice::instance().lc_main_cmd_list();
    HostBufferManager &temp_buffer = sm.host_upload_buffer();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    // Create procedural primitive if needed
    luisa::compute::ProceduralPrimitive procedural_prim;
    build_procedural_primitive(cmdlist, procedural_prim, disp_queue);

    std::lock_guard lck{_async_mtx};
    if (!procedural_prim.valid()) {
        return ~0u;// Failed to create procedural primitive
    }

    if (_height_map_surface.heightmap_idx == ~0u)
        _height_map_surface.heightmap_idx = _height_img->heap_idx();
    if (_height_map_surface.aabb_buffer_heap_idx == ~0u) {
        _height_map_surface.aabb_buffer_heap_idx = sm.bindless_allocator().allocate_buffer(
            _aabb_and_height_bounding->buffer());
    }
    _height_map_surface.height_minmax_buffer_idx = _height_map_surface.aabb_buffer_heap_idx;

    _height_map_surface.block_size.x = block_size().x;
    _height_map_surface.block_size.y = block_size().y;

    // Create procedural data variant - HeightMap uses HeightMap struct
    rbc::geometry::HeightMap height_map_data = _height_map_surface;

    // Emplace the procedural instance in AccelManager
    _procedural_instance_id = accel_manager.emplace_procedural_instance(
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        std::move(procedural_prim),
        std::move(height_map_data),
        transform,
        visibility_mask);

    return _procedural_instance_id;
}

void HeightMapResource::set_procedural_instance(
    luisa::float4x4 const &transform,
    uint8_t visibility_mask,
    bool opaque) {
    std::lock_guard lck{_async_mtx};
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();

    if (_procedural_instance_id == ~0u) {
        LUISA_WARNING("Cannot set procedural instance: instance not emplaced yet.");
        return;
    }

    accel_manager.set_procedural_instance(
        _procedural_instance_id,
        transform,
        visibility_mask,
        opaque);
}

void HeightMapResource::remove_procedural_instance() {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};
    // Deallocate bindless buffer indices
    if (_height_map_surface.aabb_buffer_heap_idx != ~0u) {
        sm.bindless_allocator().deallocate_buffer(_height_map_surface.aabb_buffer_heap_idx);
        _height_map_surface.aabb_buffer_heap_idx = ~0u;
        _height_map_surface.height_minmax_buffer_idx = ~0u;
    }

    if (_procedural_instance_id == ~0u) {
        return;// Not emplaced, nothing to do
    }

    accel_manager.remove_procedural_instance(
        buffer_allocator,
        uploader,
        disp_queue,
        _procedural_instance_id);

    _procedural_instance_id = ~0u;
}

rbc::coroutine HeightMapResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;

    auto path = this->path();
    if (path.empty()) {
        co_return;
    }

    std::lock_guard lck{_async_mtx};

    // Create device image if not exists
    if (!_height_img) {
        _height_img = new DeviceImage();
    }

    // Load height data from file
    // Assuming raw float data, loaded as single-channel float image
    _height_img->async_load_from_file(
        path,
        0,
        {},
        PixelStorage::FLOAT1,
        _resolution,
        1u,
        DeviceImage::ImageType::Float,
        false);

    // Mark procedural primitive as dirty since data changed
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
}

bool HeightMapResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;

    // Mark procedural primitive as dirty since data changed
    _procedural_prim_dirty = true;

    return true;
}

bool HeightMapResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_height_img) return false;

    auto host_data = _height_img->host_data();
    if (host_data.empty()) return false;

    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(luisa::span{host_data.data(), host_data.size_bytes()});
    return true;
}

BaseObjectType HeightMapResource::base_type() const {
    return BaseObjectType::Resource;
}

MD5 HeightMapResource::type_id() const {
    return rbc_rtti_detail::is_rtti_type<HeightMapResource>::get_md5();
}

const char *HeightMapResource::type_name() const {
    return rbc_rtti_detail::is_rtti_type<HeightMapResource>::name;
}

DECLARE_WORLD_OBJECT_REGISTER(HeightMapResource)

}// namespace rbc::world
