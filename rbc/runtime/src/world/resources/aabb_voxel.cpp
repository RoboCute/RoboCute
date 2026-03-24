#include <rbc_world/resources/aabb_voxel.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_core/utils/thread_waiter.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/fiber.h>
#include <rbc_graphics/shader_manager.h>
#include <luisa/core/logging.h>
#include <geometry/procedural_types.hpp>
#include <rbc_graphics/scene_manager.h>

namespace rbc::world {

VoxelResource::VoxelResource() {
}

VoxelResource::~VoxelResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    remove_procedural_instance();
    // Procedural primitive cleanup is handled by DisposeQueue or device destruction
    // Device resource cleanup is handled by RC
}

void VoxelResource::_upload_aabbs() {
    auto gu = GraphicsUtils::instance();
    if (!gu) return;
    // Use GraphicsUtils::update_buffer to upload data
    gu->update_buffer(_aabb_device_buffer.get(), 0, total_buffer_size_bytes());
}

void VoxelResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};
    if (_procedural_prim.valid() && (!_procedural_prim_dirty))
        return;
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Upload AABB data to device buffer
    _upload_aabbs();

    // Create procedural primitive (BLAS) with AABBs
    _procedural_prim = device.create_procedural_primitive(
        aabb_buffer(),
        luisa::compute::AccelOption{.allow_compaction = false});

    // Build the procedural primitive
    cmdlist << _procedural_prim.build();

    _procedural_prim_dirty = false;
}

uint VoxelResource::emplace_procedural_instance(
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
    build_procedural_primitive(cmdlist, disp_queue);
    std::lock_guard lck{_async_mtx};
    if (!_procedural_prim.valid()) {
        return ~0u;// Failed to create procedural primitive
    }

    // Create VoxelSurface for shader access
    // The actual buffer heap idx will be set by AccelManager
    if (_voxel_surface.aabb_buffer_heap_idx == ~0u) {
        _voxel_surface.aabb_buffer_heap_idx = sm.bindless_allocator().allocate_buffer(aabb_buffer());
    }
    _voxel_surface.aabb_buffer_offset = material_offset_bytes();

    // Emplace the procedural instance in AccelManager
    _procedural_instance_id = accel_manager.emplace_procedural_instance(
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        std::move(_procedural_prim),
        _voxel_surface,
        transform,
        visibility_mask);

    return _procedural_instance_id;
}

void VoxelResource::set_procedural_instance(
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

void VoxelResource::remove_procedural_instance() {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};

    if (_procedural_instance_id == ~0u) {
        return;// Not emplaced, nothing to do
    }
    if (_voxel_surface.aabb_buffer_heap_idx != ~0u) {
        sm.bindless_allocator().deallocate_buffer(_voxel_surface.aabb_buffer_heap_idx);
        _voxel_surface.aabb_buffer_heap_idx = ~0u;
    }
    accel_manager.remove_procedural_instance(
        buffer_allocator,
        uploader,
        disp_queue,
        _procedural_instance_id);

    _procedural_instance_id = ~0u;
}

void VoxelResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.value(_num_voxels, "num_voxels");
}

void VoxelResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    uint32_t num_voxels = 0;
    uint32_t proc_inst_id = ~0u;
    bool procedural_prim_dirty = true;

    if (ser.ar.value(num_voxels, "num_voxels")) {
        _num_voxels = num_voxels;
    }
    if (ser.ar.value(proc_inst_id, "procedural_instance_id")) {
        _procedural_instance_id = proc_inst_id;
    }
    if (ser.ar.value(procedural_prim_dirty, "procedural_prim_dirty")) {
        _procedural_prim_dirty = procedural_prim_dirty;
    }
}

bool VoxelResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return _num_voxels == 0;
}

void VoxelResource::create_empty(uint32_t num_voxels) {
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last voxel resource loading.");
    }
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    std::lock_guard lck{_async_mtx};
    _device_res.reset();
    _num_voxels = num_voxels;

    // Create device buffer with appropriate size (AABB + material data)
    if (!_aabb_device_buffer) {
        _aabb_device_buffer = new DeviceBuffer();
    } else {
        LUISA_ERROR("Create on non-empty.");
    }
    _aabb_device_buffer->create_empty(total_buffer_size_bytes(), DeviceBuffer::FileLoadType::DeviceOnly);

    // Reset VoxelSurface (will be created during emplace)
    _voxel_surface = geometry::VoxelSurface{
        .aabb_buffer_heap_idx = ~0u,
        .aabb_buffer_offset = 0,
        .mat_buffer_id = ~0u,
        .mat_buffer_offset = static_cast<uint32_t>(material_offset_bytes()),
    };

    _procedural_prim_dirty = true;

    unsafe_set_loaded();
}

luisa::span<luisa::compute::AABB const> VoxelResource::host_aabbs() const {
    if (!_aabb_device_buffer) return {};
    _aabb_device_buffer->sync_host_size_to_device();
    auto host_data = _aabb_device_buffer->host_data();
    return {reinterpret_cast<luisa::compute::AABB const *>(host_data.data()),
            _num_voxels};
}

luisa::span<luisa::compute::AABB> VoxelResource::host_aabbs() {
    if (!_aabb_device_buffer) return {};
    _aabb_device_buffer->sync_host_size_to_device();
    auto host_data = _aabb_device_buffer->host_data();
    return {reinterpret_cast<luisa::compute::AABB *>(host_data.data()),
            _num_voxels};
}

luisa::span<material::OpenPBRParticle const> VoxelResource::host_materials() const {
    if (!_aabb_device_buffer) return {};
    _aabb_device_buffer->sync_host_size_to_device();
    auto host_data = _aabb_device_buffer->host_data();
    auto mat_data = host_data.subspan(material_offset_bytes());
    return {reinterpret_cast<material::OpenPBRParticle const *>(mat_data.data()),
            _num_voxels};
}

luisa::span<material::OpenPBRParticle> VoxelResource::host_materials() {
    if (!_aabb_device_buffer) return {};
    _aabb_device_buffer->sync_host_size_to_device();
    auto host_data = _aabb_device_buffer->host_data();
    auto mat_data = host_data.subspan(material_offset_bytes());
    return {reinterpret_cast<material::OpenPBRParticle *>(mat_data.data()),
            _num_voxels};
}

luisa::compute::BufferView<luisa::compute::AABB> VoxelResource::aabb_buffer() const {
    if (!_aabb_device_buffer) return {};
    return _aabb_device_buffer->get_buffer<luisa::compute::AABB>();
}

bool VoxelResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;

    // Mark procedural primitive as dirty since data changed
    _procedural_prim_dirty = true;

    return true;
}

bool VoxelResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_aabb_device_buffer) return false;
    auto host_data = _aabb_device_buffer->host_data();
    if (host_data.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(luisa::span{host_data.data(), host_data.size_bytes()});
    return true;
}

rbc::coroutine VoxelResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;

    auto path = this->path();
    if (path.empty()) {
        co_return;
    }

    std::lock_guard lck{_async_mtx};
    _aabb_device_buffer->async_load_from_file(
        path,
        0,
        _num_voxels * sizeof(luisa::compute::AABB),
        DeviceBuffer::FileLoadType::DeviceOnly);
    if (_device_res) {
        co_return;
    }

    // Mark procedural primitive as dirty since data was loaded
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
}

BaseObjectType VoxelResource::base_type() const {
    return BaseObjectType::Resource;
}

MD5 VoxelResource::type_id() const {
    return rbc_rtti_detail::is_rtti_type<VoxelResource>::get_md5();
}

const char *VoxelResource::type_name() const {
    return rbc_rtti_detail::is_rtti_type<VoxelResource>::name;
}

DECLARE_WORLD_OBJECT_REGISTER(VoxelResource)

}// namespace rbc::world
