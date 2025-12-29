#pragma once
#include "rbc_world/component.h"
#include "rbc_world/resources/skelmesh.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc::world {

struct RBC_RUNTIME_API SkelMeshComponent final : ComponentDerive<SkelMeshComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(SkelMeshComponent)

private:
    SkelMeshComponent(Entity *entity);
    ~SkelMeshComponent();

    RC<SkelMeshResource> _skel_mesh_ref;// the animatable skeletal mesh resource
    RC<SkeletalMesh> runtime_skel_mesh; // the runtime skeletal mesh

public:
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;

    void SetRefSkelMesh(RC<SkelMeshResource> &_skel_mesh) { _skel_mesh_ref = _skel_mesh; }
public:
    void tick(float delta_time = 0.0f);
    void update_render();
    void remove_object();
};

}// namespace rbc::world

RBC_RTTI(rbc::world::SkelMeshComponent);