#include "rbc_world/components/skelmesh_component.h"
#include "rbc_world/type_register.h"

namespace rbc::world {

SkelMeshComponent::SkelMeshComponent(Entity *entity) : ComponentDerive<SkelMeshComponent>(entity) {}
SkelMeshComponent::~SkelMeshComponent() {
}
void SkelMeshComponent::on_awake() {
}
void SkelMeshComponent::on_destroy() {
}

void SkelMeshComponent::serialize_meta(ObjSerialize const &ser) const {}
void SkelMeshComponent::deserialize_meta(ObjDeSerialize const &ser) {}

DECLARE_WORLD_COMPONENT_REGISTER(SkelMeshComponent);

}// namespace rbc::world