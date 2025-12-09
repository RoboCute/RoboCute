#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
namespace rbc::world {
struct RBC_RUNTIME_API BaseObject : vstd::IOperatorNewBase {
protected:
    vstd::Guid _guid;
public:
    BaseObject();
    virtual ~BaseObject() = default;
    virtual void serialize(rbc::JsonSerializer &obj) const {}
    virtual void deserialize(rbc::JsonDeSerializer &obj) {}
    void rbc_objser(rbc::JsonSerializer &obj) const;
    void rbc_objdeser(rbc::JsonDeSerializer &obj);
    static BaseObject *create_object(vstd::Guid const &guid);
    static BaseObject *deserialize_object(rbc::JsonDeSerializer &obj);
    [[nodiscard]] virtual const char *type_name() const = 0;
    [[nodiscard]] virtual std::array<uint64_t, 2> type_id() const = 0;
};
template<typename T>
struct BaseObjectDerive : BaseObject {
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::BaseObject);