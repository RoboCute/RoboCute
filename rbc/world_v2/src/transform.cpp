#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/type_register.h>
namespace rbc::world {
luisa::vector<InstanceID> &dirty_transforms() {
    static luisa::vector<InstanceID> _singleton;
    return _singleton;
}
void Transform::rbc_objser(rbc::JsonSerializer &ser_obj) const {
    ser_obj.start_array();
    for (auto &child : _children) {
        auto &&child_guid = child->guid();
        if (child_guid) {
            ser_obj._store(child_guid);
        }
    }
    ser_obj.add_last_scope_to_object("children");
    ser_obj._store(_trs, "trs");
}
void Transform::rbc_objdeser(rbc::JsonDeSerializer &obj) {
    uint64_t size;
    if (obj.start_array(size, "children")) {
        _children.reserve(size);
        vstd::Guid child_guid;
        if (obj._load(child_guid)) {
            auto obj = get_object(child_guid);
            if (obj && obj->is_type_of(TypeInfo::get<Transform>())) {
                add_children(static_cast<Transform *>(obj));
            }
        }
        obj.end_scope();
    }
    obj._load(_trs, "trs");
    _decomposed = false;
}
void Transform::try_decompose() {
    if (_decomposed) [[likely]]
        return;
    _decomposed = true;
    auto trs = decompose(_trs);
    _position = trs.translation;
    _rotation = trs.quaternion;
    _scale = trs.scaling;
}
double3 Transform::position() {
    try_decompose();
    return _position;
}
Quaternion Transform::rotation() {
    try_decompose();
    return _rotation;
}
double3 Transform::scale() {
    try_decompose();
    return _scale;
}
void Transform::mark_dirty() {
    if (_dirty) return;
    _dirty = true;
    dirty_transforms().emplace_back(_instance_id);
}
void Transform::traversal(double4x4 const &new_trs) {
    auto old_l2w = local_to_world_matrix();
    auto old_w2l = inverse(old_l2w);
    auto transform = [&](auto &transform, Transform *tr) -> void {
        auto curr_l2w = tr->local_to_world_matrix();
        auto child_to_parent = curr_l2w * old_w2l;
        auto new_l2w = child_to_parent * new_trs;
        tr->_trs = new_l2w;
        tr->_decomposed = false;
        mark_dirty();
        for (auto &i : tr->_children) {
            transform(transform, i);
        }
    };
    for (auto &i : _children) {
        transform(transform, i);
    }
}
void Transform::set_pos(double3 const &position, bool recursive) {
    auto new_trs = rbc::rotation(position, _rotation, _scale);
    if (recursive) {
        traversal(new_trs);
    }
    _position = position;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void Transform::set_rotation(Quaternion const &rotation, bool recursive) {
    auto new_trs = rbc::rotation(_position, rotation, _scale);
    if (recursive) {
        traversal(new_trs);
    }
    _rotation = rotation;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void Transform::set_scale(double3 const &scale, bool recursive) {
    auto new_trs = rbc::rotation(_position, _rotation, scale);
    if (recursive) {
        traversal(new_trs);
    }
    _scale = scale;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void Transform::set_trs(double4x4 const &trs, bool recursive) {
    if (recursive) {
        traversal(trs);
    }
    _trs = trs;
    _decomposed = false;
    mark_dirty();
}
void Transform::set_trs(
    double3 const &position,
    Quaternion const &rotation,
    double3 const &scale,
    bool recursive) {
    auto new_trs = rbc::rotation(position, rotation, scale);
    if (recursive) {
        traversal(new_trs);
    }
    _position = position;
    _rotation = rotation;
    _scale = scale;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void Transform::add_children(Transform *tr) {
    if (tr->_parent) {
        remove_children(tr);
    }
    _children.emplace(tr);
    tr->_parent = this;
}
bool Transform::remove_children(Transform *tr) {
    auto iter = _children.find(tr);
    if (iter == _children.end()) return false;
    _children.erase(iter);
    tr->_parent = nullptr;
    return true;
}
Transform::~Transform() {
    for (auto &tr : _children) {
        LUISA_DEBUG_ASSERT(tr->_parent = this);
        tr->_parent = nullptr;
    }
    if (_parent) {
        remove_children(this);
    }
}
void dispose();
DECLARE_WORLD_TYPE_REGISTER(Transform)
}// namespace rbc::world