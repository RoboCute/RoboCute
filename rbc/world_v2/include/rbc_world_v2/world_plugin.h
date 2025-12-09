#pragma once
#include <rbc_runtime/plugin.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_core/type_info.h>

namespace rbc::world {
struct BaseObject;
struct WorldPlugin : Plugin {
    virtual BaseObject *create_object(rbc::TypeInfo const &type_info) = 0;
    virtual BaseObject *deserialize_object(rbc::JsonDeSerializer &obj) = 0;
    virtual BaseObject *get_object(InstanceID instance_id) const = 0;

    virtual ~WorldPlugin() = default;
};
}// namespace rbc::world