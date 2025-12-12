#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/hash.h>
#include <rbc_core/hash.h>
namespace rbc::world {
struct Transform;
struct BaseObject;
enum struct BaseObjectType {
    Component,
    Entity,
    Resource,
    Custom
};
struct InstanceID {
    uint64_t _placeholder;
};
RBC_WORLD_API void init_world();
RBC_WORLD_API void destroy_world();
[[nodiscard]] RBC_WORLD_API BaseObject *create_object(rbc::TypeInfo const &type_info);
[[nodiscard]] RBC_WORLD_API BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid);
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
[[nodiscard]] RBC_WORLD_API BaseObject *create_object(vstd::Guid const &type_info);
[[nodiscard]] RBC_WORLD_API BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid);
[[nodiscard]] RBC_WORLD_API BaseObject *get_object(InstanceID instance_id);
[[nodiscard]] RBC_WORLD_API BaseObject *get_object(vstd::Guid const &guid);
[[nodiscard]] RBC_WORLD_API uint64_t object_count();
[[nodiscard]] RBC_WORLD_API void dispose_all_object(vstd::Guid const &guid);
[[nodiscard]] RBC_WORLD_API BaseObjectType base_type_of(vstd::Guid const &type_id);
[[nodiscard]] RBC_WORLD_API luisa::span<InstanceID const> get_dirty_transforms();
RBC_WORLD_API void clear_dirty_transform();
RBC_WORLD_API void on_before_rendering();


template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive;
template<typename T>
struct ComponentDerive;
struct BaseObject {
    template<typename T, BaseObjectType base_type_v>
    friend struct BaseObjectDerive;
    template<typename T>
    friend struct ComponentDerive;
    friend BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid);
    friend BaseObject *create_object(vstd::Guid const &type_info);
    friend BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid);
    friend BaseObject *create_object(rbc::TypeInfo const &type_info);
protected:
    vstd::Guid _guid;
    uint64_t _instance_id{~0ull};
    BaseObject() = default;
private:
    void init();
    void init_with_guid(vstd::Guid const &guid);
public:
    [[nodiscard]] InstanceID instance_id() const {
        return InstanceID{_instance_id};
    }
    [[nodiscard]] auto guid() const { return _guid; }
    [[nodiscard]] virtual BaseObjectType base_type() const = 0;
    virtual void rbc_objser(rbc::JsonSerializer &obj) const {}
    virtual void rbc_objdeser(rbc::JsonDeSerializer &obj) {}

    [[nodiscard]] bool is_type_of(TypeInfo const &type) const {
        auto dst = type_id();
        auto &&src = type.md5();
        return src[0] == dst[0] && src[1] == dst[1];
    }
    virtual void dispose() = 0;
    [[nodiscard]] virtual const char *type_name() const = 0;
    [[nodiscard]] virtual std::array<uint64_t, 2> type_id() const = 0;
    static void *operator new(
        size_t size) noexcept {
        LUISA_ERROR("operator new banned.");
        return nullptr;
    }
    static void *operator new(
        size_t,
        void *place) noexcept {
        return place;
    }
    static void *operator new[](
        size_t size) noexcept {
        LUISA_ERROR("operator new banned.");
        return nullptr;
    }
    static void *operator new(
        size_t size, const std::nothrow_t &) noexcept {
        LUISA_ERROR("operator new banned.");
        return nullptr;
    }
    static void *operator new(
        size_t,
        void *place, const std::nothrow_t &) noexcept {
        return place;
    }
    static void *operator new[](
        size_t size, const std::nothrow_t &) noexcept {
        LUISA_ERROR("operator new banned.");
        return nullptr;
    }
    static void operator delete(
        void *pdead) noexcept {
        LUISA_ERROR("operator delete banned.");
    }
    static void operator delete(
        void *ptr,
        void *place) noexcept {
        // do nothing
    }
    static void operator delete[](
        void *pdead) noexcept {
        LUISA_ERROR("operator delete banned.");
    }
    static void operator delete(
        void *pdead, size_t) noexcept {
        LUISA_ERROR("operator delete banned.");
    }
    static void operator delete[](
        void *pdead, size_t) noexcept {
        LUISA_ERROR("operator delete banned.");
    }
    RBC_WORLD_API virtual ~BaseObject();
};
template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive : BaseObject {
    static constexpr BaseObjectType base_object_type_v = base_type_v;
private:
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
    [[nodiscard]] BaseObjectType base_type() const override {
        return base_type_v;
    }
protected:
    virtual ~BaseObjectDerive() = default;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::BaseObject);

#define DECLARE_WORLD_OBJECT_FRIEND(type_name)       \
    friend void _create_##type_name(type_name *ptr); \
    friend void _destroy_##type_name(type_name *ptr);