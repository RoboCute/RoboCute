#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/hash.h>
#include <rbc_core/rc.h>
namespace rbc::world {
enum struct BaseObjectType {
    Component,
    Entity,
    Resource,
    Custom
};
struct InstanceID {
    uint64_t _placeholder;
};
struct BaseObject {
    friend struct WorldPluginImpl;
    RBC_RC_IMPL
protected:
    vstd::Guid _guid;
    uint64_t _instance_id{~0ull};
    BaseObject();
    virtual ~BaseObject();
    static BaseObject *get_object(InstanceID instance_id);
    static BaseObject *get_object(vstd::Guid const &guid);
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
    inline void rbc_rc_delete() {
        dispose();
    }
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
};
template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive : BaseObject {
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
    [[nodiscard]] BaseObjectType base_type() const override {
        return base_type_v;
    }
};
}// namespace rbc::world
namespace luisa {
template<>
struct hash<std::array<uint64_t, 2>> {
    using T = std::array<uint64_t, 2>;
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(T const &value, uint64_t seed = hash64_default_seed) const noexcept {
        return luisa::hash64(value.data(), value.size() * sizeof(uint64_t), seed);
    }
};
}// namespace luisa
namespace std {
template<>
struct equal_to<std::array<uint64_t, 2>> {
    using T = std::array<uint64_t, 2>;
    [[nodiscard]] bool operator()(T const &a, T const &b) const noexcept {
        return a[0] == b[0] && a[1] == b[1];
    }
};
}// namespace std
RBC_RTTI(rbc::world::BaseObject);