#include <rbc_world/resources/aabb_voxel.h>
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
#include <geometry/procedural_types.hpp>
#include <rbc_graphics/scene_manager.h>

namespace rbc::world {

VoxelResource::VoxelResource() = default;

VoxelResource::~VoxelResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    remove_procedural_instance();
    // Procedural primitive cleanup is handled by DisposeQueue or device destruction
    // Device resource cleanup is handled by RC
}

void VoxelResource::_upload_aabbs(luisa::compute::CommandList &cmdlist) {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;
    auto &device = render_device->lc_device();

    // Create or resize AABB buffer if needed
    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != _num_voxels) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(_num_voxels);
    }

    // Upload AABB data to GPU buffer
    if (!_host_aabbs.empty()) {
        cmdlist << _aabb_buffer.view().copy_from(_host_aabbs.data());
    }
}

void VoxelResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Upload AABB data to device buffer
    _upload_aabbs(cmdlist);

    // Create AABB buffer if not already created
    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != _num_voxels) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(_num_voxels);
    }

    // Create procedural primitive (BLAS) with AABBs
    _procedural_prim = device.create_procedural_primitive(
        _aabb_buffer,
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

    std::lock_guard lck{_async_mtx};

    // Create procedural primitive if needed
    if (!_procedural_prim.valid() || _procedural_prim_dirty) {
        build_procedural_primitive(cmdlist, disp_queue);
    }

    if (!_procedural_prim.valid()) {
        return ~0u;// Failed to create procedural primitive
    }

    // Create VoxelSurface for shader access
    // The actual buffer heap idx will be set by AccelManager
    _voxel_surface = geometry::VoxelSurface{
        .aabb_buffer_heap_idx = sm.bindless_allocator().allocate_buffer(_aabb_buffer),// Will be set by buffer allocator
        .aabb_buffer_offset = 0};                                                     // Offset into AABB buffer

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
    sm.bindless_allocator().deallocate_buffer(_voxel_surface.aabb_buffer_heap_idx);
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

    // Resize host AABB buffer
    _host_aabbs.resize(num_voxels);

    // Reset VoxelSurface (will be created during emplace)
    _voxel_surface = geometry::VoxelSurface{};

    _procedural_prim_dirty = true;

    unsafe_set_loaded();
}

void VoxelResource::set_aabbs(luisa::span<luisa::compute::AABB const> aabbs) {
    std::lock_guard lck{_async_mtx};
    _num_voxels = static_cast<uint32_t>(aabbs.size());
    _host_aabbs.resize(_num_voxels);
    if (!aabbs.empty()) {
        std::memcpy(_host_aabbs.data(), aabbs.data(), aabbs.size() * sizeof(luisa::compute::AABB));
    }
    _procedural_prim_dirty = true;
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
    if (_host_aabbs.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(luisa::span{
        reinterpret_cast<std::byte const *>(_host_aabbs.data()),
        _host_aabbs.size() * sizeof(luisa::compute::AABB)});
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
    if (_device_res) {
        co_return;
    }

    // Mark procedural primitive as dirty since data was loaded
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
}

DECLARE_WORLD_OBJECT_REGISTER(VoxelResource)

}// namespace rbc::world
