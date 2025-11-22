#pragma once

#include "rbc_core/ecs.h"
#include "rbc_runtime/system.h"

namespace rbc {

struct RBC_RUNTIME_API TransformComponent : public IComponent {
    DECLARE_COMPONENT(TransformComponent);

    // Local transform
    luisa::float3 position{0.f, 0.f, 0.f};
    luisa::float4 rotation{0.f, 0.f, 0.f, 1.f};// Quaternion (x, y, z, w)
    luisa::float3 scaling{1.f, 1.f, 1.f};

    // Hierarchy
    EntityID parent = INVALID_ENTITY;
    luisa::vector<EntityID> children;

    // Cached global transform (world space)
    mutable luisa::float4x4 global_transform{luisa::make_float4x4(1.f)};
    mutable bool global_dirty = true;

    // Helper methods
    void compute_local_matrix(luisa::float4x4 &out_matrix) const;
    void mark_dirty();

    // interface
public:
    [[nodiscard]] luisa::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer<IComponent> &serializer) const override;
    void deserialize(DeSerializer<IComponent> &deserializer) override;
};

class RBC_RUNTIME_API TransformSystem : public ISystem {
public:
    explicit TransformSystem(EntityManager *entity_manager)
        : entity_manager_(entity_manager) {}

    // Interface
    void update(float delta_time) override;
    [[nodiscard]] int get_priority() const override { return 100; }
    [[nodiscard]] bool is_parallel_safe() const override { return false; }// Hierarchy requires serial update

    // Update transform
    void update_transforms(luisa::span<const EntityID> entities);

private:
    EntityManager *entity_manager_;

    // Helper methods
    void organize_by_depth(const luisa::vector<EntityID> &entities,
                           luisa::vector<luisa::vector<EntityID>> &layers);
    void update_single_transform(EntityID entity);
};

}// namespace rbc