#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/hash.h>
#include <rbc_core/hash.h>
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
template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive;
template<typename T>
struct ComponentDerive;
struct WorldPluginImpl;
struct BaseObjectBase {
    template<typename T, BaseObjectType base_type_v>
    friend struct BaseObjectDerive;
    template<typename T>
    friend struct ComponentDerive;
    friend struct WorldPluginImpl;
protected:
    vstd::Guid _guid;
    uint64_t _instance_id{~0ull};
    BaseObjectBase() = default;
    virtual ~BaseObjectBase() = default;
private:
    void init();
    void init_with_guid(vstd::Guid const &guid);
    virtual void _dispose_self() = 0;
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
};
struct BaseObject : BaseObjectBase {
private:
    void _dispose_self() override;
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
    ~BaseObjectDerive() {
        static_cast<BaseObjectBase *>(this)->_dispose_self();
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::BaseObject);