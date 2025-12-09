#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
namespace rbc::world {
enum struct BaseObjectType {
    Component,
    Entity,
    Custom
};
struct InstanceID {
    uint64_t _placeholder;
};
struct BaseObject {
    friend struct WorldPluginImpl;
protected:
    vstd::Guid _guid;
    uint64_t _instance_id;
    BaseObject();
    virtual ~BaseObject();
    static BaseObject *get_object(InstanceID instance_id);
public:
    InstanceID instance_id() const {
        return InstanceID{_instance_id};
    }
    virtual BaseObjectType base_type() const = 0;
    virtual void serialize(rbc::JsonSerializer &obj) const {}
    virtual void deserialize(rbc::JsonDeSerializer &obj) {}
    virtual void rbc_objser(rbc::JsonSerializer &obj) const = 0;
    virtual void rbc_objdeser(rbc::JsonDeSerializer &obj) = 0;
    [[nodiscard]] virtual bool is_type_of(TypeInfo const &type) const = 0;
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
struct BaseObjectImpl : BaseObject {
private:
    void rbc_objser(rbc::JsonSerializer &obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
    [[nodiscard]] bool is_type_of(TypeInfo const &type) const override;
};
template<typename T, BaseObjectType base_type_v>
struct BaseObjectDerive : BaseObjectImpl {
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
RBC_RTTI(rbc::world::BaseObject);