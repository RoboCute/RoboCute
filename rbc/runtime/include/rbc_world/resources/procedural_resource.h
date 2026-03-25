#pragma once
#include <rbc_world/resource_base.h>
#include <luisa/runtime/rtx/procedural_primitive.h>

namespace rbc {
struct AccelManager;
struct DisposeQueue;
}// namespace rbc

namespace rbc::world {

/// Interface for procedural resources that can be used in ray tracing
/// Provides a unified interface for procedural primitive management
/// Derived classes should inherit from both ProceduralResource and ResourceBaseImpl<Derived>
struct RBC_RUNTIME_API ProceduralResource : Resource {
protected:
    ProceduralResource() = default;
    virtual ~ProceduralResource() = default;

public:
    /// Check if procedural primitive needs to be rebuilt
    [[nodiscard]] virtual bool is_procedural_dirty() const = 0;
    /// Get the procedural instance ID in AccelManager
    [[nodiscard]] virtual uint32_t procedural_instance_id() const = 0;

    /// Build/create the procedural primitive from current bounds/data
    /// Creates the AABB buffer and ProceduralPrimitive BLAS
    virtual void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        luisa::compute::ProceduralPrimitive &procedural_prim,
        DisposeQueue &disp_queue) = 0;

    /// Emplace this resource as a procedural instance in the acceleration structure
    /// Creates the procedural primitive if needed and adds it to AccelManager
    /// Returns the instance ID, or ~0u on failure
    [[nodiscard]] virtual uint emplace_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu) = 0;

    /// Update the procedural instance transform and visibility
    virtual void set_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = false) = 0;

    /// Remove this resource from the acceleration structure
    virtual void remove_procedural_instance() = 0;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::ProceduralResource)
