#pragma once
#include <rbc_runtime/plugin.h>
#include <rbc_world_v2/base_object.h>
#include <rbc_core/type_info.h>

namespace rbc::world {
struct Transform;
struct BaseObject;
struct WorldPlugin : Plugin {

    [[nodiscard]] virtual BaseObject *create_object(rbc::TypeInfo const &type_info) = 0;
    [[nodiscard]] virtual BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid) = 0;
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<BaseObject, T>)
    T *create_object() {
        return static_cast<T *>(create_object(TypeInfo::get<T>()));
    }
    template<typename T>
        requires(rbc_rtti_detail::is_rtti_type<T>::value && std::is_base_of_v<BaseObject, T>)
    T *create_object_with_guid(vstd::Guid const &guid) {
        return static_cast<T *>(create_object_with_guid(TypeInfo::get<T>(), guid));
    }
    [[nodiscard]] virtual BaseObject *create_object(vstd::Guid const &type_info) = 0;
    [[nodiscard]] virtual BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid) = 0;
    [[nodiscard]] virtual BaseObject *get_object(InstanceID instance_id) const = 0;
    [[nodiscard]] virtual BaseObject *get_object(vstd::Guid const &guid) const = 0;
    [[nodiscard]] virtual uint64_t object_count() const = 0;
    [[nodiscard]] virtual void dispose_all_object(vstd::Guid const &guid) = 0;
    [[nodiscard]] virtual BaseObjectType base_type_of(vstd::Guid const &type_id) const = 0;
    [[nodiscard]] virtual luisa::span<InstanceID const> get_dirty_transforms() const = 0;
    virtual void clear_dirty_transform() = 0;
    virtual void on_before_rendering() = 0;
protected:
    ~WorldPlugin() = default;
};
}// namespace rbc::world