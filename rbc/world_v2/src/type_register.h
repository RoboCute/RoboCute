#pragma once
#include <rbc_world_v2/base_object.h>
#include <luisa/vstl/pool.h>
namespace rbc::world {
BaseObject *get_object(InstanceID instance_id);
BaseObject *get_object(vstd::Guid const &guid);
template<typename T, typename Impl>
concept RegistableWorldObject = std::is_base_of_v<BaseObject, T> && std::is_default_constructible_v<Impl> && std::is_base_of_v<T, Impl>;
struct TypeRegisterBase;
using CreateFunc = vstd::func_ptr_t<BaseObject *()>;
struct TypeRegisterBase {
    BaseObjectType base_obj_type;
    TypeRegisterBase *p_next{};
    virtual std::array<uint64_t, 2> type_id() = 0;
    virtual BaseObject *create() = 0;
};
void type_regist_init_mark(TypeRegisterBase *type_register);
template<typename T, typename Impl>
    requires RegistableWorldObject<T, Impl>
struct TypeRegister : TypeRegisterBase {
    vstd::spin_mutex _mtx;
    vstd::Pool<Impl, true> _pool;
    TypeRegister(size_t init_capa = 64)
        : _pool(init_capa, false) {
        TypeRegisterBase::base_obj_type = T::base_object_type_v;
        type_regist_init_mark(this);
    }
    std::array<uint64_t, 2> type_id() override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
    BaseObject *create() override {
        return _pool.create_lock(_mtx);
    }
    void destroy(Impl *ptr) {
        _pool.destroy_lock(_mtx, ptr);
    }
};
#define DECLARE_TYPE_REGISTER(type_name)                                   \
    static TypeRegister<type_name, type_name##Impl> type_name##_register_; \
    void type_name##Impl::dispose() {                                      \
        type_name##_register_.destroy(this);                               \
    }
}// namespace rbc::world