#pragma once
#include "rbc_world/component.h"
#include "rbc_world/resources/skelmesh.h"

namespace rbc::world {

struct RBC_RUNTIME_API SkelMeshComponent final : ComponentDerive<SkelMeshComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(SkelMeshComponent)

private:
    SkelMeshComponent(Entity *entity);
    ~SkelMeshComponent();

    RC<SkelMeshResource> _skel_mesh_ref;

public:
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::SkelMeshComponent);