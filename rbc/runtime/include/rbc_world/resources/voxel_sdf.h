#pragma once
#include <rbc_world/resource_base.h>
#include <rbc_world/resources/procedural_resource.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/rtx/procedural_primitive.h>
#include <luisa/runtime/rtx/aabb.h>
#include "rbc_core/buffer.h"
#include <rbc_graphics/device_assets/device_volume.h>

namespace rbc {
struct DeviceResource;
struct AccelManager;
struct BufferAllocator;
struct BufferUploader;
struct DisposeQueue;
struct HostBufferManager;
namespace geometry {
struct SDFMap;
}// namespace geometry
}// namespace rbc

namespace rbc::world {

/// Resource class for SDF (Signed Distance Field) Voxel data
/// Stores 3D signed distance field data for GPU-accelerated rendering and ray marching
struct RBC_RUNTIME_API SDFVoxelResource final : ProceduralResource {
    DECLARE_WORLD_OBJECT_FRIEND(SDFVoxelResource)
    using BaseType = ResourceBaseImpl<SDFVoxelResource>;
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Resource;

private:
    RC<DeviceResource> _device_res;
    mutable rbc::shared_atomic_mutex _async_mtx;

    // SDF grid dimensions
    uint3 _grid_size{};

    // Host-side SDF data stored as float array
    // Layout: 3D grid of float distances stored in flat array
    // Indexing: data[x + y * size.x + z * size.x * size.y]
    luisa::vector<std::byte> _host_data;
    RC<DeviceVolume> _device_volume;

    // SDF bounds in world space
    float3 _uvw_scale{1.0f, 1.0f, 1.0f};
    float3 _uvw_offset{0.0f, 0.0f, 0.0f};

    // Maximum ray march samples for this SDF
    uint32_t _sample_count{256};

    // Procedural primitive for ray tracing integration
    luisa::compute::ProceduralPrimitive _procedural_prim;
    luisa::compute::Buffer<luisa::compute::AABB> _aabb_buffer;
    uint32_t _procedural_instance_id{~0u};///< Instance ID in AccelManager
    bool _procedural_prim_dirty{true};    ///< Flag to indicate if AABB needs rebuild

    SDFVoxelResource();
    ~SDFVoxelResource();

    /// Compute AABB for the entire SDF volume
    void _compute_aabb(CommandList &cmdlist);

public:
    /// Check if resource is empty (no SDF data loaded)
    [[nodiscard]] bool empty() const;

    /// Get grid dimensions
    [[nodiscard]] auto grid_size() const { return _grid_size; }

    /// Get total number of voxels
    [[nodiscard]] uint64_t num_voxels() const {
        return static_cast<uint64_t>(_grid_size.x) * _grid_size.y * _grid_size.z;
    }

    /// Get the total size of SDF data in bytes
    [[nodiscard]] uint64_t host_data_size_bytes() const { return _host_data.size(); }

    /// Get raw host data span as floats
    [[nodiscard]] luisa::span<float const> host_data() const {
        return {reinterpret_cast<float const *>(_host_data.data()), _host_data.size() / sizeof(float)};
    }
    [[nodiscard]] luisa::span<float> host_data() {
        return {reinterpret_cast<float *>(_host_data.data()), _host_data.size() / sizeof(float)};
    }

    /// Get UVW scale (world space bounds)
    [[nodiscard]] auto uvw_scale() const { return _uvw_scale; }

    /// Get UVW offset (world space origin)
    [[nodiscard]] auto uvw_offset() const { return _uvw_offset; }

    /// Get sample count for ray marching
    [[nodiscard]] auto sample_count() const { return _sample_count; }

    /// Set UVW scale
    void set_uvw_scale(float3 scale) { _uvw_scale = scale; }

    /// Set UVW offset
    void set_uvw_offset(float3 offset) { _uvw_offset = offset; }

    /// Set sample count
    void set_sample_count(uint32_t count) { _sample_count = count; }

    /// Get the device-side volume object
    [[nodiscard]] DeviceVolume *device_volume() const;

    /// Create empty SDF voxel resource with specified grid dimensions
    void create_empty(uint3 grid_size);

    /// Serialize metadata for persistence
    void serialize_meta(ObjSerialize const &ser) const override;

    /// Deserialize metadata from storage
    void deserialize_meta(ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    // Procedural primitive interface for ray tracing integration

    /// Check if this SDF voxel has a procedural primitive created
    [[nodiscard]] bool has_procedural_primitive() const override { return _procedural_prim.valid(); }

    /// Check if procedural primitive needs to be rebuilt
    [[nodiscard]] bool is_procedural_dirty() const override { return _procedural_prim_dirty; }

    /// Get the procedural primitive (valid after emplace_procedural_instance or build_procedural_primitive)
    [[nodiscard]] luisa::compute::ProceduralPrimitive const &procedural_primitive() const override { return _procedural_prim; }

    /// Get the procedural instance ID in AccelManager
    [[nodiscard]] uint32_t procedural_instance_id() const override { return _procedural_instance_id; }

    /// Build/create the procedural primitive from current SDF bounds
    /// Creates the AABB buffer and ProceduralPrimitive BLAS
    void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        DisposeQueue &disp_queue) override;

    /// Emplace this SDF voxel as a procedural instance in the acceleration structure
    /// Creates the procedural primitive if needed and adds it to AccelManager
    /// Returns the instance ID, or ~0u on failure
    [[nodiscard]] uint emplace_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu) override;

    /// Update the procedural instance transform and visibility
    void set_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = false) override;

    /// Remove this SDF voxel from the acceleration structure
    void remove_procedural_instance() override;

protected:
    bool _install() override;
    bool unsafe_save_to_path() const override;

public:
    [[nodiscard]] BaseObjectType base_type() const override;
    [[nodiscard]] MD5 type_id() const override;
    [[nodiscard]] const char *type_name() const override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::SDFVoxelResource)
