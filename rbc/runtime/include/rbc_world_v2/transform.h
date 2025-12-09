#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/vector.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_core/quaternion.h>
namespace rbc::world {
struct Entity;
struct RBC_RUNTIME_API Transform final : BaseObjectDerive<Transform> {
private:
    luisa::vector<Transform *> _children;
    double3 _position;
    double3 _local_scale;
    Quaternion _rotation;
    bool _dirty{};

public:
    Transform();
    void serialize(rbc::JsonSerializer &obj) const override;
    void deserialize(rbc::JsonDeSerializer &obj) override;
    double4x4 local_to_world_matrix() const;
    ~Transform();
};
}// namespace rbc::world
RBC_RTTI(rbc::world::Transform);