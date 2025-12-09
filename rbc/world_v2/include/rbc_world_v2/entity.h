#pragma once
#include <rbc_core/type_info.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/component.h>
namespace rbc::world {
struct Entity : BaseObjectDerive<Entity, BaseObjectType::Entity> {
protected:
    luisa::unordered_map<TypeInfo, InstanceID> _components;
public:
    // virtual void add_component(InstanceID)
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);