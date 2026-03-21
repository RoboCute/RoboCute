#pragma once
#include <rbc_world/resource_base.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/rtx/procedural_primitive.h>
#include <luisa/runtime/rtx/aabb.h>
#include "rbc_core/buffer.h"
#include <rbc_graphics/device_assets/device_buffer.h>

namespace rbc {
struct DeviceResource;
struct DeviceGaussianSplat;
struct AccelManager;
struct BufferAllocator;
struct BufferUploader;
struct DisposeQueue;
struct HostBufferManager;
namespace geometry {
struct VoxelSurface;
struct SDFMap;
}// namespace geometry
#include <geometry/gaussian_probe.hpp>
}// namespace rbc
namespace rbc::world {

/// Resource class for 3D Gaussian Splatting data
/// Stores Gaussian parameters loaded from PLY files for GPU-accelerated rendering
struct RBC_RUNTIME_API GaussianSplatResource final : ResourceBaseImpl<GaussianSplatResource> {
    DECLARE_WORLD_OBJECT_FRIEND(GaussianSplatResource)
    using BaseType = ResourceBaseImpl<GaussianSplatResource>;

private:
    RC<DeviceResource> _device_res;
    mutable rbc::shared_atomic_mutex _async_mtx;

    // Gaussian data parameters
    uint32_t _num_gaussians{};
    uint32_t _sh_degree{};///< Spherical harmonics degree (0-3)

    // Host-side Gaussian data stored in a single flattened buffer
    // Layout: [positions (3N floats)] [features (N*F floats)] [opacity (N floats)]
    //         [scale (3N floats)] [rotation (4N floats)]
    // where F = (sh_degree+1)^2 * 3, N = _num_gaussians
    DeviceBuffer _device_buffer;

    // Offsets (in floats) for each component within _host_data
    uint64_t _pos_offset{0};    ///< Position offset (always 0)
    uint64_t _feature_offset{0};///< Feature offset
    uint64_t _opacity_offset{0};///< Opacity offset
    uint64_t _scale_offset{0};  ///< Scale offset
    uint64_t _rotq_offset{0};   ///< Rotation offset

    /// Calculate offsets based on _num_gaussians and _sh_degree
    void _update_offsets();

    // Procedural primitive for ray tracing integration
    luisa::compute::ProceduralPrimitive _procedural_prim;
    luisa::compute::Buffer<luisa::compute::AABB> _aabb_buffer;
    uint32_t _procedural_instance_id{~0u};///< Instance ID in AccelManager
    bool _procedural_prim_dirty{true};    ///< Flag to indicate if AABB needs rebuild

    GaussianSplatResource();
    ~GaussianSplatResource();

    /// Compute per-Gaussian AABBs from positions and scales
    /// Uses fiber parallelism for large Gaussian counts
    void _compute_all_aabbs(CommandList &cmdlist);
    void _assert_size_align(uint64_t size) const;

public:
    /// Decode Gaussian splat data from a PLY file
    bool decode(luisa::filesystem::path const &path);

    /// Check if resource is empty (no Gaussians loaded)
    [[nodiscard]] bool empty() const;

    /// Get number of Gaussians
    [[nodiscard]] auto num_gaussians() const { return _num_gaussians; }

    /// Get spherical harmonics degree
    [[nodiscard]] auto sh_degree() const { return _sh_degree; }
    /// Get the total number of floats in host data
    [[nodiscard]] uint64_t host_data_size_bytes() const { return _device_buffer.host_data().size(); }

    /// Get raw host data span
    [[nodiscard]] luisa::span<GaussianProbe const> host_data() const {
        auto byte_buffer = _device_buffer.host_data();
        _assert_size_align(byte_buffer.size_bytes());
        return {reinterpret_cast<GaussianProbe const *>(byte_buffer.data()), byte_buffer.size_bytes() / sizeof(GaussianProbe)};
    }
    [[nodiscard]] luisa::span<GaussianProbe> host_data() {
        auto byte_buffer = _device_buffer.host_data();
        _assert_size_align(byte_buffer.size_bytes());
        return {reinterpret_cast<GaussianProbe *>(byte_buffer.data()), byte_buffer.size_bytes() / sizeof(GaussianProbe)};
    }

    /// Calculate total size of Gaussian data in bytes
    [[nodiscard]] uint64_t data_size_bytes() const;

    /// Get the device-side Gaussian splat object
    [[nodiscard]] DeviceGaussianSplat *device_gaussian_splat() const;

    /// Create empty Gaussian splat resource with specified parameters
    void create_empty(uint32_t num_gaussians, uint32_t sh_degree);

    /// Serialize metadata for persistence
    void serialize_meta(ObjSerialize const &ser) const override;

    /// Deserialize metadata from storage
    void deserialize_meta(ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    // Procedural primitive interface for ray tracing integration

    /// Check if this Gaussian splat has a procedural primitive created
    [[nodiscard]] bool has_procedural_primitive() const { return _procedural_prim.valid(); }

    /// Check if procedural primitive needs to be rebuilt
    [[nodiscard]] bool is_procedural_dirty() const { return _procedural_prim_dirty; }

    /// Get the procedural primitive (valid after emplace_procedural_instance or build_procedural_primitive)
    [[nodiscard]] luisa::compute::ProceduralPrimitive const &procedural_primitive() const { return _procedural_prim; }

    /// Get the procedural instance ID in AccelManager
    [[nodiscard]] auto procedural_instance_id() const { return _procedural_instance_id; }

    /// Build/create the procedural primitive from current Gaussian bounds
    /// Creates the AABB buffer (one per Gaussian) and ProceduralPrimitive BLAS
    void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        DisposeQueue &disp_queue);

    /// Emplace this Gaussian splat as a procedural instance in the acceleration structure
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

    /// Remove this Gaussian splat from the acceleration structure
    void remove_procedural_instance();

protected:
    bool _install() override;
    bool unsafe_save_to_path() const override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::GaussianSplatResource)
