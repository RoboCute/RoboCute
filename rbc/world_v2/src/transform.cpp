#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include "type_register.h"
namespace rbc::world {
struct TransformImpl : Transform {
    TransformImpl() {
    }
    void serialize(rbc::JsonSerializer &obj) const override {
        obj._store(_position, "position");
        obj._store(_local_scale, "local_scale");
        obj._store(_rotation.v, "rotation");
    }
    void deserialize(rbc::JsonDeSerializer &obj) override {
        obj._load(_position, "position");
        obj._load(_local_scale, "local_scale");
        obj._load(_rotation.v, "rotation");
    }
    double4x4 local_to_world_matrix() const override {
        return rbc::rotation(_position, _rotation, _local_scale);
    }
    ~TransformImpl() {
        for (auto &i : _children) {
            auto obj = get_object(i);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
            static_cast<Component *>(obj)->entity()->dispose();
        }
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Transform)
}// namespace rbc::world