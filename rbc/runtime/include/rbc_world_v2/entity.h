#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct RBC_RUNTIME_API Entity : BaseObjectDerive<Entity> {
    Entity();
    ~Entity();
    void rbc_objser(rbc::JsonSerializer &obj) const;
    void rbc_objdeser(rbc::JsonDeSerializer &obj);
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);