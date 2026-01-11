#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <rbc_core/shared_atomic_mutex.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/hash.h>
#include <luisa/core/logging.h>
#include <rbc_core/hash.h>
#include <rbc_core/rc.h>
#include <rbc_core/serde.h>
namespace rbc::world {
struct BaseObject;
struct Entity;
enum struct BaseObjectType {
    None,
    Component,
    Entity,
    Resource,
    Custom
};
struct InstanceID {
    uint64_t _placeholder;
    constexpr void set_invalid() {
        _placeholder = ~0ull;
    }
    [[nodiscard]] constexpr bool is_invalid() const {
        return _placeholder == ~0ull;
    }
    [[nodiscard]] constexpr operator bool() const {
        return _placeholder != ~0ull;
    }
    [[nodiscard]] constexpr bool operator==(InstanceID i) const {
        return _placeholder == i._placeholder;
    }
    constexpr static InstanceID invalid_resource_handle() {
        return {~0ull};
    }
};
RBC_RUNTIME_API void init_world(luisa::filesystem::path const &meta_path = {}, luisa::filesystem::path const &binary_path = {});
RBC_RUNTIME_API void destroy_world();
[[nodiscard]] RBC_RUNTIME_API BaseObject *create_object(rbc::TypeInfo const &type_info);
[[nodiscard]] RBC_RUNTIME_API BaseObjectType get_base_object_type(vstd::Guid const &type_id);
[[nodiscard]] RBC_RUNTIME_API BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid);
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
[[nodiscard]] RBC_RUNTIME_API BaseObject *create_object(vstd::Guid const &type_info);
[[nodiscard]] RBC_RUNTIME_API BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid);
[[nodiscard]] RBC_RUNTIME_API BaseObject *_zz_create_object_with_guid_test_base(vstd::Guid const &type_info, vstd::Guid const &guid, BaseObjectType desire_type);
[[nodiscard]] RBC_RUNTIME_API BaseObject *get_object(InstanceID instance_id);
[[nodiscard]] RBC_RUNTIME_API BaseObject *get_object(vstd::Guid const &guid);
[[nodiscard]] RBC_RUNTIME_API RC<BaseObject> get_object_ref(InstanceID instance_id);
[[nodiscard]] RBC_RUNTIME_API RC<BaseObject> get_object_ref(vstd::Guid const &guid);
[[nodiscard]] RBC_RUNTIME_API uint64_t object_count();
[[nodiscard]] RBC_RUNTIME_API BaseObjectType base_type_of(vstd::Guid const &type_id);
RBC_RUNTIME_API void _zz_clear_dirty_transform();
RBC_RUNTIME_API void _zz_on_before_rendering();
RBC_RUNTIME_API bool world_transform_dirty();

template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive;
template<typename T>
struct ComponentDerive;
struct ObjSerialize {
    rbc::ArchiveWrite &ar;
};
struct ObjDeSerialize {
    rbc::ArchiveRead &ar;
};
struct BaseObject : RCBase {
    template<typename T, BaseObjectType base_type_v>
    friend struct BaseObjectDerive;
    template<typename T>
    friend struct ComponentDerive;
    friend struct Entity;
    friend struct BaseObjectStatics;
    RBC_RUNTIME_API friend BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid);
    RBC_RUNTIME_API friend BaseObject *create_object(vstd::Guid const &type_info);
    RBC_RUNTIME_API friend BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid);
    RBC_RUNTIME_API friend BaseObject *create_object(rbc::TypeInfo const &type_info);
    RBC_RUNTIME_API friend BaseObject *_zz_create_object_with_guid_test_base(vstd::Guid const &type_info, vstd::Guid const &guid, BaseObjectType desire_type);
protected:
    BaseObject() = default;
private:
    vstd::Guid _guid;
    uint64_t _instance_id{~0ull};
    void init();
    void init_with_guid(vstd::Guid const &guid);

public:
    [[nodiscard]] InstanceID instance_id() const {
        return InstanceID{_instance_id};
    }
    [[nodiscard]] auto guid() const { return _guid; }
    [[nodiscard]] virtual BaseObjectType base_type() const = 0;

    virtual void serialize_meta(ObjSerialize const &obj) const {
    }
    virtual void deserialize_meta(ObjDeSerialize const &obj) {
    }

    [[nodiscard]] bool is_type_of(TypeInfo const &type) const {
        auto dst = type_id();
        auto &&src = type.md5();
        return src == dst;
    }
    [[nodiscard]] virtual const char *type_name() const = 0;
    [[nodiscard]] virtual vstd::MD5 type_id() const = 0;
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
    RBC_RUNTIME_API virtual ~BaseObject();
};
template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive : BaseObject {
    static constexpr BaseObjectType base_object_type_v = base_type_v;
public:
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] vstd::MD5 type_id() const override {
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

#define DECLARE_WORLD_OBJECT_FRIEND(type_name)             \
    friend void ea525e13_create_##type_name(type_name *);  \
    friend void ea525e13_destroy_##type_name(type_name *); \
    void rbc_rc_delete() override;

#define DECLARE_WORLD_COMPONENT_FRIEND(type_name)                   \
    friend void ea525e13_create_##type_name(type_name *, Entity *); \
    friend void ea525e13_destroy_##type_name(type_name *);          \
    void rbc_rc_delete() override;

namespace luisa {
template<>
struct hash<rbc::world::InstanceID> {
    size_t operator()(rbc::world::InstanceID const &inst, uint64_t seed = luisa::hash64_default_seed) const {
        return luisa::hash64(&inst, sizeof(rbc::world::InstanceID), seed);
    }
};
}// namespace luisa
namespace std {
template<>
struct hash<rbc::world::InstanceID> {
    size_t operator()(rbc::world::InstanceID const &inst) const {
        return luisa::hash64(&inst, sizeof(rbc::world::InstanceID), luisa::hash64_default_seed);
    }
};
}// namespace std
namespace vstd {
template<>
struct hash<rbc::world::InstanceID> {
    size_t operator()(rbc::world::InstanceID const &inst) const {
        return luisa::hash64(&inst, sizeof(rbc::world::InstanceID), luisa::hash64_default_seed);
    }
};
template<>
struct compare<rbc::world::InstanceID> {
    int operator()(rbc::world::InstanceID const &a, rbc::world::InstanceID const &b) const {
        if (a._placeholder < b._placeholder) return -1;
        if (a._placeholder > b._placeholder) return 1;
        return 0;
    }
};

}// namespace vstd