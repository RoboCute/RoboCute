#include <rbc_world/components/transform_component.h>
#include <rbc_world/entity.h>
#include <rbc_world/type_register.h>
#include <rbc_core/runtime_static.h>

namespace rbc::world {
struct TransformStatic : RBCStruct {
    luisa::vector<InstanceID> dirty_trans;
};
TransformComponent::TransformComponent() {}
static RuntimeStatic<TransformStatic> _trans_inst;
luisa::vector<InstanceID> &dirty_transforms() {
    return _trans_inst->dirty_trans;
}
void TransformComponent::serialize_meta(ObjSerialize const &obj) const {
    obj.ar.start_array();
    for (auto &child : _children) {
        auto &&child_guid = child->guid();
        if (child_guid) {
            obj.ar.value(child_guid);
        }
    }
    obj.ar.end_array("children");
    obj.ar.value(_trs, "trs");
}
float4x4 TransformComponent::trs_float() const {
    return make_float4x4(
        make_float4(_trs[0]),
        make_float4(_trs[1]),
        make_float4(_trs[2]),
        make_float4(_trs[3]));
}
void TransformComponent::deserialize_meta(ObjDeSerialize const &deser) {
    uint64_t size;
    if (deser.ar.start_array(size, "children")) {
        _children.reserve(size);
        vstd::Guid child_guid;
        if (deser.ar.value(child_guid)) {
            auto obj = get_object(child_guid);
            if (obj && obj->is_type_of(TypeInfo::get<TransformComponent>())) {
                add_children(static_cast<TransformComponent *>(obj));
            }
        }
        deser.ar.end_scope();
    }
    deser.ar.value(_trs, "trs");
    _decomposed = false;
}
void TransformComponent::try_decompose() {
    if (_decomposed) [[likely]]
        return;
    _decomposed = true;
    auto trs = decompose(_trs);
    _position = trs.translation;
    _rotation = normalize(trs.quaternion);
    _scale = trs.scaling;
}
double3 TransformComponent::position() {
    try_decompose();
    return _position;
}
Quaternion TransformComponent::rotation() {
    try_decompose();
    return _rotation;
}
double3 TransformComponent::scale() {
    try_decompose();
    return _scale;
}
void TransformComponent::mark_dirty() {
    if (_dirty) return;
    _dirty = true;
    dirty_transforms().emplace_back(instance_id());
}
void TransformComponent::traversal(double4x4 const &new_trs) {
    auto old_l2w = _trs;
    auto old_w2l = inverse(old_l2w);
    auto transform = [&](auto &transform, TransformComponent *tr) -> void {
        auto curr_l2w = tr->_trs;
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
void TransformComponent::set_pos(double3 const &position, bool recursive) {
    try_decompose();
    auto new_trs = rbc::rotation(position, _rotation, _scale);
    if (recursive) {
        traversal(new_trs);
    }
    _position = position;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void TransformComponent::set_rotation(Quaternion const &rotation, bool recursive) {
    try_decompose();
    auto new_trs = rbc::rotation(_position, rotation, _scale);
    if (recursive) {
        traversal(new_trs);
    }
    _rotation = rotation;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void TransformComponent::set_scale(double3 const &scale, bool recursive) {
    try_decompose();
    auto new_trs = rbc::rotation(_position, _rotation, scale);
    if (recursive) {
        traversal(new_trs);
    }
    _scale = scale;
    _trs = new_trs;
    _decomposed = true;
    mark_dirty();
}
void TransformComponent::set_trs(double4x4 const &trs, bool recursive) {
    if (recursive) {
        traversal(trs);
    }
    _trs = trs;
    _decomposed = false;
    mark_dirty();
}
void TransformComponent::set_trs(
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
void TransformComponent::add_children(TransformComponent *tr) {
    if (tr->_parent) {
        remove_children(tr);
    }
    _children.emplace(tr);
    tr->_parent = this;
}
bool TransformComponent::remove_children(TransformComponent *tr) {
    auto iter = _children.find(tr);
    if (iter == _children.end()) return false;
    _children.erase(iter);
    tr->_parent = nullptr;
    return true;
}
TransformComponent::~TransformComponent() {
    for (auto &tr : _children) {
        LUISA_DEBUG_ASSERT(tr->_parent = this);
        tr->_parent = nullptr;
    }
    if (_parent) {
        remove_children(this);
    }
}
void TransformComponent::add_on_update_event(Component *ptr, void (Component::*func_ptr)()) {
    _on_update_events.force_emplace(ptr->instance_id(), func_ptr);
}

void TransformComponent::_execute_on_update_event() {
    luisa::vector<InstanceID> invalid_components;
    for (auto &i : _on_update_events) {
        auto obj = get_object(i.first);
        if (!obj || obj->base_type() != BaseObjectType::Component) [[unlikely]] {
            invalid_components.emplace_back(i.first);
        }
        auto ptr = static_cast<Component *>(obj);
        (ptr->*i.second)();
    }
    for (auto &i : invalid_components) {
        _on_update_events.remove(i);
    }
}

void TransformComponent::remove_on_update_event(Component *ptr) {
    _on_update_events.remove(ptr->instance_id());
}
// clang-format off
DECLARE_WORLD_OBJECT_REGISTER(TransformComponent)
// clang-format on
}// namespace rbc::world