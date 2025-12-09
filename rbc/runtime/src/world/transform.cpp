#include <rbc_world_v2/transform.h>
namespace rbc::world {
Transform::Transform() {}
void Transform::serialize(rbc::JsonSerializer &obj) const {
    obj._store(_position, "position");
    obj._store(_local_scale, "local_scale");
    obj._store(_rotation.v, "rotation");
}
void Transform::deserialize(rbc::JsonDeSerializer &obj) {
    obj._load(_position, "position");
    obj._load(_local_scale, "local_scale");
    obj._load(_rotation.v, "rotation");
}
Transform::~Transform() {
}
double4x4 Transform::local_to_world_matrix() const {
    return rbc::rotation(_position, _rotation, _local_scale);
}
}// namespace rbc::world