#pragma once
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <luisa/vstl/common.h>
namespace rbc::world {
struct BaseObject : vstd::IOperatorNewBase {
    friend struct WorldPluginImpl;
protected:
    vstd::Guid _guid;
    BaseObject();
    virtual ~BaseObject() = default;
public:
    virtual void serialize(rbc::JsonSerializer &obj) const {}
    virtual void deserialize(rbc::JsonDeSerializer &obj) {}
    virtual void rbc_objser(rbc::JsonSerializer &obj) const = 0;
    virtual void rbc_objdeser(rbc::JsonDeSerializer &obj) = 0;
    virtual void dispose() = 0;
    [[nodiscard]] virtual const char *type_name() const = 0;
    [[nodiscard]] virtual std::array<uint64_t, 2> type_id() const = 0;
};
struct BaseObjectImpl : BaseObject {
private:
    void rbc_objser(rbc::JsonSerializer &obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
};
template<typename T>
struct BaseObjectDerive : BaseObjectImpl {
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<T>::name;
    }
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
};
}// namespace rbc::world
RBC_RTTI(rbc::world::BaseObject);