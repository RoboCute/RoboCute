#pragma once
#include <rbc_world/component.h>

namespace rbc::world {
struct JoltComponent final : ComponentDerive<JoltComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(JoltComponent)
private:
    void *_physics_instance{};
    JoltComponent();
    ~JoltComponent();
public:
    void init(
        bool is_floor// for demo, floor or object
    );
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
    void update_pos();
    static void update_step(float delta_time);
};
}// namespace rbc::world

RBC_RTTI(rbc::world::JoltComponent)