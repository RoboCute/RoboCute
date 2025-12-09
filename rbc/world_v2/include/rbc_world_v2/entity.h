#pragma once
#include <rbc_world_v2/base_object.h>
namespace rbc::world {
struct Entity : BaseObjectDerive<Entity> {
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Entity);