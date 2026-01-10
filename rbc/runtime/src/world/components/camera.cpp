#include <rbc_world/components/camera_component.h>
#include <rbc_world/type_register.h>

namespace rbc::world {
CameraComponent::CameraComponent(Entity *entity)
    : ComponentDerive<CameraComponent>{entity} {}
CameraComponent::~CameraComponent() {}
void CameraComponent::serialize_meta(ObjSerialize const &obj) const {

}
void CameraComponent::deserialize_meta(ObjDeSerialize const &obj) {
    
}
DECLARE_WORLD_COMPONENT_REGISTER(CameraComponent)
}// namespace rbc::world