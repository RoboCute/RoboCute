#pragma once
#include <rbc_runtime/plugin.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_core/type_info.h>

namespace rbc::world {
struct Transform;
struct BaseObject;
struct WorldPlugin : Plugin {
    [[nodiscard]] virtual BaseObject *create_object(rbc::TypeInfo const &type_info) = 0;
    [[nodiscard]] virtual BaseObject *deserialize_object(rbc::JsonDeSerializer &obj) = 0;
    [[nodiscard]] virtual BaseObject *get_object(InstanceID instance_id) const = 0;
    [[nodiscard]] virtual luisa::span<InstanceID const> get_dirty_transforms() const = 0;
    virtual void clear_dirty_transform() const = 0;
protected:
    ~WorldPlugin() = default;
};
}// namespace rbc::world