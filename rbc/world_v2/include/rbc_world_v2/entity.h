#pragma once
#include <rbc_core/type_info.h>
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Component;
struct Entity : BaseObjectDerive<Entity, BaseObjectType::Entity> {
protected:
    luisa::unordered_map<std::array<uint64_t, 2>, InstanceID> _components;
public:
    virtual bool add_component(Component* component) = 0;
    virtual bool remove_component(TypeInfo const& type) = 0;
    virtual Component* get_component(TypeInfo const& type) = 0;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);