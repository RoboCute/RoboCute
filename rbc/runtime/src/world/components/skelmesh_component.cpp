#include "rbc_world/components/skelmesh_component.h"
#include "rbc_world/type_register.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc::world {

SkelMeshComponent::SkelMeshComponent(Entity *entity) : ComponentDerive<SkelMeshComponent>(entity) {}
SkelMeshComponent::~SkelMeshComponent() {
}
void SkelMeshComponent::on_awake() {
    // called when entity->_add_component was called
    runtime_skel_mesh = RC<SkeletalMesh>::New();
}
void SkelMeshComponent::on_destroy() {
    // called when Component->_clear_entity() was called
    remove_object();
}

void SkelMeshComponent::serialize_meta(ObjSerialize const &ser) const {
}
void SkelMeshComponent::deserialize_meta(ObjDeSerialize const &ser) {}

void SkelMeshComponent::tick(float delta_time) {
    // LUISA_INFO("Updating SkelMesh");
    if (!runtime_skel_mesh) [[unlikely]] {
        LUISA_ERROR("Ticking A SkelMeshComponent with an Invalid Runtime SkelMesh");
        return;
    }
    if (!_skel_mesh_ref->loaded()) [[unlikely]] {
        LUISA_ERROR("Ticking A SkelMeshComponent with an Unloaded Static SkelMesh Resource");
        return;
    }

    // Now Everything Ready, start Initialize
    if (!runtime_skel_mesh->IsInitialized()) {
        runtime_skel_mesh->ref_skelmesh = _skel_mesh_ref;

        runtime_skel_mesh->InitAnim();
    }

    // Now Initialized, Start Tick
    runtime_skel_mesh->Tick(delta_time);
}

void SkelMeshComponent::update_render() {
    // LUISA_INFO("UpdateRender SkelMesh");
}

void SkelMeshComponent::remove_object() {
}

DECLARE_WORLD_COMPONENT_REGISTER(SkelMeshComponent);

}// namespace rbc::world