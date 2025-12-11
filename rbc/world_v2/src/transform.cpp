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
    void rbc_objser(rbc::JsonSerializer &ser_obj) const override {
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
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override {
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
    void try_decompose() {
        if (_decomposed) [[likely]]
            return;
        _decomposed = true;
        auto trs = decompose(_trs);
        _position = trs.translation;
        _rotation = trs.quaternion;
        _scale = trs.scaling;
    }
    double3 position() override {
        try_decompose();
        return _position;
    }
    Quaternion rotation() override {
        try_decompose();
        return _rotation;
    }
    double3 scale() override {
        try_decompose();
        return _scale;
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
            tr->_trs = new_l2w;
            tr->_decomposed = false;
            static_cast<TransformImpl *>(tr)->mark_dirty();
            for (auto &i : tr->_children) {
                transform(transform, i);
            }
        };
        for (auto &i : _children) {
            transform(transform, i);
        }
    }
    void set_pos(double3 const &position, bool recursive) override {
        auto new_trs = rbc::rotation(position, _rotation, _scale);
        if (recursive) {
            traversal(new_trs);
        }
        _position = position;
        _trs = new_trs;
        _decomposed = true;
        mark_dirty();
    }
    void set_rotation(Quaternion const &rotation, bool recursive) override {
        auto new_trs = rbc::rotation(_position, rotation, _scale);
        if (recursive) {
            traversal(new_trs);
        }
        _rotation = rotation;
        _trs = new_trs;
        _decomposed = true;
        mark_dirty();
    }
    void set_scale(double3 const &scale, bool recursive) override {
        auto new_trs = rbc::rotation(_position, _rotation, scale);
        if (recursive) {
            traversal(new_trs);
        }
        _scale = scale;
        _trs = new_trs;
        _decomposed = true;
        mark_dirty();
    }
    void set_trs(double4x4 const &trs, bool recursive) override {
        if (recursive) {
            traversal(trs);
        }
        _trs = trs;
        _decomposed = false;
        mark_dirty();
    }
    void set_trs(
        double3 const &position,
        Quaternion const &rotation,
        double3 const &scale,
        bool recursive) override {
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
    void add_children(Transform *tr) override {
        if (tr->_parent) {
            static_cast<TransformImpl *>(tr->_parent)->remove_children(tr);
        }
        _children.emplace(tr);
        tr->_parent = this;
    }
    bool remove_children(Transform *tr) override {
        _children.erase(tr);
    }
    ~TransformImpl() {
        for (auto &tr : _children) {
            LUISA_DEBUG_ASSERT(tr->_parent = this);
            tr->_parent = nullptr;
        }
        if (_parent) {
            static_cast<TransformImpl *>(_parent)->remove_children(this);
        }
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Transform)
}// namespace rbc::world