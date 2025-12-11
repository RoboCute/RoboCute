#pragma once
#include <rbc_graphics/object_types.h>
#include <rbc_world_v2/resource_base.h>
namespace rbc::world {
struct Material : ResourceBaseImpl<Material> {
protected:
    MaterialStub::MatDataType _mat_data_type;
public:

};
};// namespace rbc::world
RBC_RTTI(rbc::world::Material)