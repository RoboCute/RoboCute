#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/vector.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/component.h>
#include <rbc_core/quaternion.h>
namespace rbc::world {
struct Entity;
struct Transform : ComponentDerive<Transform> {
protected:
    luisa::vector<InstanceID> _children;
    double3 _position;
    double3 _local_scale{1, 1, 1};
    Quaternion _rotation;
    bool _dirty{};
public:
    [[nodiscard]] virtual double4x4 local_to_world_matrix() const = 0;
    
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Transform);