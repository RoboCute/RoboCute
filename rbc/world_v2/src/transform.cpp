#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include "type_register.h"
namespace rbc::world {
luisa::vector<InstanceID> &dirty_transforms() {
    static luisa::vector<InstanceID> _singleton;
    return _singleton;
}
struct TransformImpl : Transform {
    TransformImpl() {
    }
    void serialize(rbc::JsonSerializer &ser_obj) const override {
        ser_obj.start_array();
        for (auto &i : _children) {
            auto obj = get_object(i);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<Transform>()));
            auto child = static_cast<Transform *>(obj);
            static_cast<BaseObject *>(child)->rbc_objser(ser_obj);
        }
        ser_obj.add_last_scope_to_object("children");
        ser_obj._store(_position, "position");
        ser_obj._store(_scale, "scale");
        ser_obj._store(_rotation.v, "rotation");
    }
    void deserialize(rbc::JsonDeSerializer &obj) override {
        obj._load(_position, "position");
        obj._load(_scale, "scale");
        obj._load(_rotation.v, "rotation");
    }
    double4x4 local_to_world_matrix() const override {
        return rbc::rotation(_position, _rotation, _scale);
    }
    void mark_dirty() {
        if (_dirty) return;
        _dirty = true;
        dirty_transforms().emplace_back(_instance_id);
    }
    void traversal(double4x4 const &new_trs) {
        auto old_l2w = local_to_world_matrix();
        auto old_w2l = inverse(old_l2w);
        auto transform = [&](auto &transform, Transform *tr) -> void {
            auto curr_l2w = tr->local_to_world_matrix();
            auto child_to_parent = curr_l2w * old_w2l;
            auto new_l2w = child_to_parent * new_trs;
            auto trs = decompose(new_l2w);
            tr->_position = trs.translation;
            tr->_rotation = trs.quaternion;
            tr->_scale = trs.scaling;
            for (size_t i = 0; i < tr->_children.size(); ++i) {
                auto obj = get_object(tr->_children[i]);
                if (!obj) {
                    tr->_children.erase(tr->_children.begin() + i);
                    --i;
                    continue;
                }
                transform(transform, static_cast<Transform *>(obj));
            }
        };
        for (size_t i = 0; i < _children.size(); ++i) {
            auto obj = get_object(_children[i]);
            if (!obj) {
                _children.erase(_children.begin() + i);
                --i;
                continue;
            }
            transform(transform, static_cast<Transform *>(obj));
        }
    }
    void set_pos(double3 const &position, bool recursive) override {
        if (recursive) {
            auto new_trs = rbc::rotation(position, _rotation, _scale);
            traversal(new_trs);
        }
        _position = position;
        mark_dirty();
    }
    void set_rotation(Quaternion const &rotation, bool recursive) override {
        if (recursive) {
            auto new_trs = rbc::rotation(_position, rotation, _scale);
            traversal(new_trs);
        }
        _rotation = rotation;
        mark_dirty();
    }
    void set_scale(double3 const &scale, bool recursive) override {
        if (recursive) {
            auto new_trs = rbc::rotation(_position, _rotation, scale);
            traversal(new_trs);
        }
        _scale = scale;
        mark_dirty();
    }
    void set_trs(
        double3 const &position,
        Quaternion const &rotation,
        double3 const &scale,
        bool recursive) override {
        if (recursive) {
            auto new_trs = rbc::rotation(position, rotation, scale);
            traversal(new_trs);
        }
        _position = position;
        _rotation = rotation;
        _scale = scale;
        mark_dirty();
    }
    void add_children(Transform *tr) override {
#ifndef NDEBUG
        for (auto &i : _children) {
            if (get_object(i) == tr) [[unlikely]] {
                LUISA_ERROR("Transform already exists.");
            }
        }
#endif
    }
    void remove_children_at(uint64_t index) override {
        LUISA_DEBUG_ASSERT(index < _children.size());
        _children.erase(_children.begin() + index);
    }
    bool remove_children(Transform *tr) override {
        for (uint64_t i = 0; i < _children.size(); ++i) {
            auto obj = get_object(_children[i]);
            if (obj == tr) {
                _children.erase(_children.begin() + i);
                return true;
            }
        }
        return false;
    }
    ~TransformImpl() {
        for (auto &i : _children) {
            auto obj = get_object(i);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<Transform>()));
            auto tr = static_cast<Transform *>(obj);
            LUISA_DEBUG_ASSERT(tr->_parent = this);
            tr->_parent = _parent;
        }
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Transform)
}// namespace rbc::world