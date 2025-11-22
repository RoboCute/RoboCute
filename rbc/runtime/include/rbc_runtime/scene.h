#pragma once

#include "rbc_core/ecs.h"
#include "rbc_runtime/system.h"

namespace rbc {

struct TransformComponent : public IComponent {
    DECLARE_COMPONENT(TransformComponent);
    // local transform // 当前为了debug方便只实现scaling
    luisa::float3 scaling;

    // interface
public:
    [[nodiscard]] luisa::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer<IComponent> &serializer) const override;
    void deserialize(DeSerializer<IComponent> &deserializer) override;
};

class TransformSystem : public ISystem {
public:// interface
    void update(float delta_time) override;
    [[nodiscard]] int get_priority() const override { return 100; }
    [[nodiscard]] bool is_parallel_safe() const override { return true; }

    // update transform
    void update_transforms(luisa::span<const EntityID> entities);

private:
    EntityManager *entity_manager_;
};

}// namespace rbc