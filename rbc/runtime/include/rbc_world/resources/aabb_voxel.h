#pragma once
#include <rbc_world/resource_base.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/rtx/procedural_primitive.h>
#include <luisa/runtime/rtx/aabb.h>

namespace rbc {
#include <geometry/procedural_types.hpp>
struct DeviceResource;
struct AccelManager;
struct BufferAllocator;
struct BufferUploader;
struct DisposeQueue;
struct HostBufferManager;
}// namespace rbc

namespace rbc::world {

/// Resource class for AABB-based Voxel data
/// Stores axis-aligned bounding boxes for GPU-accelerated voxel rendering
struct RBC_RUNTIME_API VoxelResource final : ResourceBaseImpl<VoxelResource> {
    DECLARE_WORLD_OBJECT_FRIEND(VoxelResource)
    using BaseType = ResourceBaseImpl<VoxelResource>;

private:
    RC<DeviceResource> _device_res;
    mutable rbc::shared_atomic_mutex _async_mtx;

    // Number of voxel instances (AABB count)
    uint32_t _num_voxels{};

    // Host-side AABB data
    luisa::vector<luisa::compute::AABB> _host_aabbs;

    // Device-side AABB buffer
    luisa::compute::Buffer<luisa::compute::AABB> _aabb_buffer;

    // VoxelSurface for shader access
    geometry::VoxelSurface _voxel_surface;

    // Procedural primitive for ray tracing integration
    luisa::compute::ProceduralPrimitive _procedural_prim;
    uint32_t _procedural_instance_id{~0u};///< Instance ID in AccelManager
    bool _procedural_prim_dirty{true};    ///< Flag to indicate if AABB needs rebuild

    VoxelResource();
    ~VoxelResource();

    /// Upload host AABB data to device buffer
    void _upload_aabbs(luisa::compute::CommandList &cmdlist);

public:
    /// Check if resource is empty (no voxels loaded)
    [[nodiscard]] bool empty() const;

    /// Get number of voxels
    [[nodiscard]] auto num_voxels() const { return _num_voxels; }

    /// Get the total size of AABB data in bytes
    [[nodiscard]] uint64_t host_data_size_bytes() const { return _host_aabbs.size() * sizeof(luisa::compute::AABB); }

    /// Get raw host AABB data span
    [[nodiscard]] luisa::span<luisa::compute::AABB const> host_aabbs() const { return _host_aabbs; }
    [[nodiscard]] luisa::span<luisa::compute::AABB> host_aabbs() { return _host_aabbs; }

    /// Get the device-side AABB buffer
    [[nodiscard]] luisa::compute::Buffer<luisa::compute::AABB> const &aabb_buffer() const { return _aabb_buffer; }

    /// Get the VoxelSurface data for shader access
    [[nodiscard]] geometry::VoxelSurface const &voxel_surface() const { return _voxel_surface; }
    [[nodiscard]] geometry::VoxelSurface &voxel_surface() { return _voxel_surface; }

    /// Create empty voxel resource with specified number of voxels
    void create_empty(uint32_t num_voxels);

    /// Set AABB data from host buffer
    void set_aabbs(luisa::span<luisa::compute::AABB const> aabbs);

    /// Serialize metadata for persistence
    void serialize_meta(ObjSerialize const &ser) const override;

    /// Deserialize metadata from storage
    void deserialize_meta(ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    // Procedural primitive interface for ray tracing integration

    /// Check if this voxel resource has a procedural primitive created
    [[nodiscard]] bool has_procedural_primitive() const { return _procedural_prim.valid(); }

    /// Check if procedural primitive needs to be rebuilt
    [[nodiscard]] bool is_procedural_dirty() const { return _procedural_prim_dirty; }

    /// Get the procedural primitive (valid after emplace_procedural_instance or build_procedural_primitive)
    [[nodiscard]] luisa::compute::ProceduralPrimitive const &procedural_primitive() const { return _procedural_prim; }

    /// Get the procedural instance ID in AccelManager
    [[nodiscard]] auto procedural_instance_id() const { return _procedural_instance_id; }

    /// Build/create the procedural primitive from current AABB data
    /// Creates the AABB buffer and ProceduralPrimitive BLAS
    void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        DisposeQueue &disp_queue);

    /// Emplace this voxel resource as a procedural instance in the acceleration structure
    /// Creates the procedural primitive if needed and adds it to AccelManager
    /// Returns the instance ID, or ~0u on failure
    [[nodiscard]] uint emplace_procedural_instance(
        luisa::float4x4 const &transform = luisa::float4x4{},
        uint8_t visibility_mask = 0xffu);

    /// Update the procedural instance transform and visibility
    void set_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = false);

    /// Remove this voxel resource from the acceleration structure
    void remove_procedural_instance();

protected:
    bool _install() override;
    bool unsafe_save_to_path() const override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::VoxelResource)
