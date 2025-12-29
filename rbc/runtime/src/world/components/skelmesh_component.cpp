#include "rbc_world/components/skelmesh_component.h"
#include "rbc_world/type_register.h"

namespace rbc::world {

SkelMeshComponent::SkelMeshComponent(Entity *entity) : ComponentDerive<SkelMeshComponent>(entity) {}
SkelMeshComponent::~SkelMeshComponent() {
}
void SkelMeshComponent::on_awake() {
    // called when entity->_add_component was called
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
}
void SkelMeshComponent::update_render() {
    // LUISA_INFO("UpdateRender SkelMesh");
}

void SkelMeshComponent::remove_object() {
}

DECLARE_WORLD_COMPONENT_REGISTER(SkelMeshComponent);

}// namespace rbc::world