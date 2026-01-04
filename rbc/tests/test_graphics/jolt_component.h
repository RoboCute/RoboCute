#pragma once
#include <rbc_world/component.h>

namespace rbc::world {
struct JoltComponent final : ComponentDerive<JoltComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(JoltComponent)
private:
    void *_physics_instance{};
    JoltComponent(Entity *entity);
    ~JoltComponent();
public:
    static void test_jolt();
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
};
}// namespace rbc::world

RBC_RTTI(rbc::world::JoltComponent)