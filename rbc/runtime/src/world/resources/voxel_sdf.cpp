#include <rbc_world/resources/voxel_sdf.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_core/utils/thread_waiter.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/fiber.h>
#include <rbc_graphics/shader_manager.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/scene_manager.h>

namespace rbc::world {

SDFVoxelResource::SDFVoxelResource() = default;

SDFVoxelResource::~SDFVoxelResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    // Procedural primitive cleanup is handled by DisposeQueue or device destruction
    // Device resource cleanup is handled by RC
}

void SDFVoxelResource::_compute_aabb(CommandList &cmdlist) {
    // For SDF volumes, we compute a single AABB that encompasses the entire volume
    // The AABB is computed from uvw_offset and uvw_scale
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;
    auto &device = render_device->lc_device();

    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != 1) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(1);
    }

    // Compute AABB min/max from uvw_offset and uvw_scale
    float3 aabb_min = _uvw_offset;
    float3 aabb_max = _uvw_offset + _uvw_scale;

    luisa::compute::AABB host_aabb;
    host_aabb.packed_min = {aabb_min.x, aabb_min.y, aabb_min.z};
    host_aabb.packed_max = {aabb_max.x, aabb_max.y, aabb_max.z};

    // Upload AABB data to GPU buffer
    cmdlist << _aabb_buffer.view().copy_from(&host_aabb);
}

void SDFVoxelResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    luisa::compute::ProceduralPrimitive &procedural_prim,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Compute AABB for the volume
    _compute_aabb(cmdlist);

    // Create AABB buffer (single AABB for the entire volume)
    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != 1) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(1);
    }

    // Create procedural primitive (BLAS) with single AABB for the volume
    _procedural_prim = device.create_procedural_primitive(
        _aabb_buffer,
        luisa::compute::AccelOption{.allow_compaction = false});

    // Build the procedural primitive
    cmdlist << _procedural_prim.build();

    _procedural_prim_dirty = false;
}

uint SDFVoxelResource::emplace_procedural_instance(
    luisa::float4x4 const &transform,
    uint8_t visibility_mask) {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    luisa::compute::CommandList &cmdlist = RenderDevice::instance().lc_main_cmd_list();
    HostBufferManager &temp_buffer = sm.host_upload_buffer();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};

    // Create procedural primitive if needed
    if (!_procedural_prim.valid() || _procedural_prim_dirty) {
        build_procedural_primitive(cmdlist, _procedural_prim, disp_queue);
    }

    if (!_procedural_prim.valid()) {
        return ~0u;// Failed to create procedural primitive
    }

    // Create procedural data variant - SDF volumes use SDFMap
    geometry::SDFMap sdf_map{
        .volume_idx = _device_volume ? _device_volume->heap_idx() : ~0u,
        .sample_count = _sample_count,
        .uvw_scale = _uvw_scale,
        .uvw_offset = _uvw_offset};

    // Emplace the procedural instance in AccelManager
    _procedural_instance_id = accel_manager.emplace_procedural_instance(
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        std::move(_procedural_prim),
        std::move(sdf_map),
        transform,
        visibility_mask);

    return _procedural_instance_id;
}

void SDFVoxelResource::set_procedural_instance(
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

void SDFVoxelResource::remove_procedural_instance() {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};

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

void SDFVoxelResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.value(_grid_size, "grid_size");
    ser.ar.value(_uvw_scale, "uvw_scale");
    ser.ar.value(_uvw_offset, "uvw_offset");
    ser.ar.value(_sample_count, "sample_count");
    ser.ar.value(_procedural_instance_id, "procedural_instance_id");
    ser.ar.value(_procedural_prim_dirty, "procedural_prim_dirty");
}

void SDFVoxelResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    uint3 grid_size{};
    float3 uvw_scale{1.0f, 1.0f, 1.0f};
    float3 uvw_offset{0.0f, 0.0f, 0.0f};
    uint32_t sample_count = 256;
    uint32_t proc_inst_id = ~0u;
    bool procedural_prim_dirty = true;

    if (ser.ar.value(grid_size, "grid_size")) {
        _grid_size = grid_size;
    }
    if (ser.ar.value(uvw_scale, "uvw_scale")) {
        _uvw_scale = uvw_scale;
    }
    if (ser.ar.value(uvw_offset, "uvw_offset")) {
        _uvw_offset = uvw_offset;
    }
    if (ser.ar.value(sample_count, "sample_count")) {
        _sample_count = sample_count;
    }
    if (ser.ar.value(proc_inst_id, "procedural_instance_id")) {
        _procedural_instance_id = proc_inst_id;
    }
    if (ser.ar.value(procedural_prim_dirty, "procedural_prim_dirty")) {
        _procedural_prim_dirty = procedural_prim_dirty;
    }
}

bool SDFVoxelResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return _host_data.empty() && !_device_volume;
}

DeviceVolume *SDFVoxelResource::device_volume() const {
    std::shared_lock lck{_async_mtx};
    return _device_volume.get();
}

void SDFVoxelResource::create_empty(uint3 grid_size) {
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last SDF voxel loading.");
    }
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    std::lock_guard lck{_async_mtx};
    _device_res.reset();
    _device_volume.reset();
    _grid_size = grid_size;

    // Calculate total size needed (one float per voxel)
    uint64_t num_voxels = static_cast<uint64_t>(grid_size.x) * grid_size.y * grid_size.z;
    uint64_t total_bytes = num_voxels * sizeof(float);

    // Resize host buffer to fit all data
    _host_data.resize(total_bytes);

    _procedural_prim_dirty = true;

    // Note: Device volume would be created separately during install
    unsafe_set_loaded();
}

bool SDFVoxelResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;

    auto &device = render_device->lc_device();

    // Create device volume if host data exists
    if (!_host_data.empty() && !_device_volume) {
        _device_volume = rbc::RC<DeviceVolume>::New();
        _device_volume->create_volume<float>(
            device,
            PixelStorage::FLOAT1,// Single channel float for SDF
            _grid_size,
            1);// Single mip level
    }

    // Mark procedural primitive as dirty since data changed
    _procedural_prim_dirty = true;

    return true;
}

bool SDFVoxelResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    auto data = host_data();
    if (data.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(luisa::span{
        reinterpret_cast<std::byte const *>(data.data()),
        data.size() * sizeof(float)});
    return true;
}

rbc::coroutine SDFVoxelResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;

    auto path = this->path();
    if (path.empty()) {
        co_return;
    }

    std::lock_guard lck{_async_mtx};
    if (_device_res) {
        co_return;
    }

    // For now, just mark as installed
    // Actual file loading would be implemented based on specific file format

    // Mark procedural primitive as dirty since data was loaded
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
}

BaseObjectType SDFVoxelResource::base_type() const {
    return BaseObjectType::Resource;
}

MD5 SDFVoxelResource::type_id() const {
    return rbc_rtti_detail::is_rtti_type<SDFVoxelResource>::get_md5();
}

const char *SDFVoxelResource::type_name() const {
    return rbc_rtti_detail::is_rtti_type<SDFVoxelResource>::name;
}

DECLARE_WORLD_OBJECT_REGISTER(SDFVoxelResource)

}// namespace rbc::world
