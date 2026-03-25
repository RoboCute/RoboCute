#pragma once
#include <rbc_world/resource_base.h>
#include <rbc_world/resources/procedural_resource.h>
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
#include <material/gs_mat.hpp>
#include <geometry/gaussian_probe.hpp>
#include <geometry/procedural_types.hpp>
}// namespace rbc
namespace rbc::world {

/// Spherical Harmonic coefficients for Gaussian splatting
/// Number of coefficients depends on SH degree: (degree + 1)^2 coefficients per channel
/// Stored as 3 channels (RGB) of float coefficients
struct SphereHarmonic {

    /// Get the number of floats per Gaussian for given SH degree
    [[nodiscard]] static constexpr uint32_t num_floats(uint32_t sh_degree) {
        return (sh_degree + 1) * (sh_degree + 1) * 3;
    }
    /// Get the size in bytes for given SH degree
    [[nodiscard]] static constexpr uint32_t size_bytes(uint32_t sh_degree) {
        return num_floats(sh_degree) * sizeof(float);
    }
};

/// Resource class for 3D Gaussian Splatting data
/// Stores Gaussian parameters loaded from PLY files for GPU-accelerated rendering
///
/// Memory Layout (Structure of Arrays):
/// The _device_buffer uses a SoA layout for efficient GPU access:
/// [0]: array<GaussianProbe, _num_gaussians>
/// [1]: array<SphereHarmonic, _num_gaussians> (size depends on _sh_degree)
/// [2]: array<OpenPBRParticle, _num_gaussians>
struct RBC_RUNTIME_API GaussianSplatResource final : ProceduralResource {
    DECLARE_WORLD_OBJECT_FRIEND(GaussianSplatResource)
    using BaseType = ResourceBaseImpl<GaussianSplatResource>;
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Resource;

private:
    geometry::GaussianSplatingGeometry surface;
    RC<DeviceResource> _device_res;
    mutable rbc::shared_atomic_mutex _async_mtx;
    // Gaussian data parameters
    uint32_t _num_gaussians{};

    uint32_t _sh_degree{0};

    /// Offsets for structure-of-arrays layout in _device_buffer

    DeviceBuffer _device_buffer;

    /// AABB buffer for procedural primitive (one AABB per Gaussian)
    luisa::compute::Buffer<luisa::compute::AABB> _aabb_buffer;

    uint32_t _procedural_instance_id{~0u};///< Instance ID in AccelManager
    bool _procedural_prim_dirty{true};    ///< Flag to indicate if AABB needs rebuild

    GaussianSplatResource();
    ~GaussianSplatResource();

    /// Compute per-Gaussian AABBs from positions and scales
    /// Uses fiber parallelism for large Gaussian counts
    void _compute_all_aabbs(CommandList &cmdlist);
    void _assert_size_align(uint64_t size) const;
    static constexpr uint64_t _size_align(uint64_t size) {
        return (size + 15ull) & (~15ull);
    }

public:

    /// Check if resource is empty (no Gaussians loaded)
    [[nodiscard]] bool empty() const;

    /// Get number of Gaussians
    [[nodiscard]] auto num_gaussians() const { return _num_gaussians; }

    /// Get SH degree
    [[nodiscard]] auto sh_degree() const { return _sh_degree; }

    /// Get total size in bytes
    [[nodiscard]] uint64_t total_size_bytes() const;
    /// Get the size of GaussianProbe array in bytes
    [[nodiscard]] uint64_t probe_data_size_bytes() const {
        return _size_align(static_cast<uint64_t>(_num_gaussians) * sizeof(GaussianProbe));
    }

    /// Get the size of SphereHarmonic array in bytes
    [[nodiscard]] uint64_t sh_data_size_bytes() const {
        return _size_align(static_cast<uint64_t>(_num_gaussians) * SphereHarmonic::size_bytes(_sh_degree));
    }

    /// Get the size of OpenPBRParticle array in bytes
    [[nodiscard]] uint64_t material_data_size_bytes() const {
        return _size_align(static_cast<uint64_t>(_num_gaussians) * sizeof(material::OpenPBRParticle));
    }

    /// Get offset to SH data
    [[nodiscard]] auto sh_offset() const { return probe_data_size_bytes(); }

    /// Get offset to material data
    [[nodiscard]] auto material_offset() const { return sh_offset() + sh_data_size_bytes(); }

    /// Host-view getters (read-only access to host data)
    /// Returns span to GaussianProbe array
    [[nodiscard]] luisa::span<GaussianProbe const> host_probes() const;
    [[nodiscard]] luisa::span<GaussianProbe> host_probes();

    /// Returns span to SH coefficients as raw floats
    /// Size = _num_gaussians * (sh_degree + 1)^2 * 3 floats
    [[nodiscard]] luisa::span<float const> host_sh_coeffs() const;
    [[nodiscard]] luisa::span<float> host_sh_coeffs();

    /// Returns span to OpenPBRParticle array
    [[nodiscard]] luisa::span<material::OpenPBRParticle const> host_materials() const;
    [[nodiscard]] luisa::span<material::OpenPBRParticle> host_materials();

    /// Returns the underlying device buffer
    [[nodiscard]] DeviceBuffer const &device_buffer() const { return _device_buffer; }

    /// Returns the AABB buffer for procedural primitive
    [[nodiscard]] luisa::compute::Buffer<luisa::compute::AABB> const &aabb_buffer() const { return _aabb_buffer; }

    /// Create empty Gaussian splat resource with specified parameters
    void create_empty(uint32_t num_gaussians, uint32_t sh_degree);

    /// Serialize metadata for persistence
    void serialize_meta(ObjSerialize const &ser) const override;

    /// Deserialize metadata from storage
    void deserialize_meta(ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    /// Check if procedural primitive needs to be rebuilt
    [[nodiscard]] bool is_procedural_dirty() const override { return _procedural_prim_dirty; }

    /// Get the procedural instance ID in AccelManager
    [[nodiscard]] uint32_t procedural_instance_id() const override { return _procedural_instance_id; }

    /// Build/create the procedural primitive from current Gaussian bounds
    /// Creates the AABB buffer (one per Gaussian) and ProceduralPrimitive BLAS
    void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        luisa::compute::ProceduralPrimitive &procedural_prim,
        DisposeQueue &disp_queue) override;

    /// Emplace this Gaussian splat as a procedural instance in the acceleration structure
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

    /// Remove this Gaussian splat from the acceleration structure
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

RBC_RTTI(rbc::world::GaussianSplatResource)
