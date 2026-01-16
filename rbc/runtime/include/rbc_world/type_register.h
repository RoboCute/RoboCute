#pragma once
#include <rbc_world/base_object.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/pool.h>
#include <luisa/vstl/spin_mutex.h>
namespace rbc::world {
struct TypeRegisterBase;
using CreateFunc = vstd::func_ptr_t<BaseObject *()>;
struct TypeRegisterBase {
    friend struct BaseObjectStatics;
    virtual BaseObject *create() = 0;
private:
    TypeRegisterBase *p_next{};
    virtual MD5 type_id() = 0;
    virtual void init() = 0;
    virtual void destroy() = 0;
protected:
    BaseObjectType base_obj_type;
    TypeRegisterBase() = default;
    RBC_RUNTIME_API void _base_init();
    ~TypeRegisterBase() = default;
public:
    [[nodiscard]] BaseObjectType base_type() const { return base_obj_type; }
};

template<typename T>
    requires std::is_base_of_v<BaseObject, T>
struct TypeObjectRegister : TypeRegisterBase {
    vstd::spin_mutex _mtx;
    vstd::optional<vstd::Pool<vstd::Storage<T>, true>> _pool;
    vstd::func_ptr_t<void(T *)> _ctor_func;
    vstd::func_ptr_t<void(T *)> _dtor_func;
    TypeObjectRegister(
        vstd::func_ptr_t<void(T *)> ctor_func,
        vstd::func_ptr_t<void(T *)> dtor_func)
        : _ctor_func(ctor_func),
          _dtor_func(dtor_func) {
        TypeRegisterBase::base_obj_type = T::base_object_type_v;
        _base_init();
    }
    void init() override {
        _pool.create(2, false);
    }
    void destroy() override {
        _pool.destroy();
    }
    MD5 type_id() override {
        return rbc_rtti_detail::is_rtti_type<T>::get_md5();
    }
    BaseObject *create() override {
        auto ptr = reinterpret_cast<T *>(_pool->create_lock(_mtx));
        _ctor_func(ptr);
        return static_cast<BaseObject *>(ptr);
    }
    void destroy(T *ptr) {
        _dtor_func(ptr);
        _pool->destroy_lock(_mtx, reinterpret_cast<vstd::Storage<T> *>(ptr));
    }
};

#define DECLARE_WORLD_OBJECT_REGISTER(type_name)                              \
    void ea525e13_create_##type_name(type_name *ptr) {                        \
        new (std::launder(ptr)) type_name{};                                  \
    }                                                                         \
    void ea525e13_destroy_##type_name(type_name *ptr) {                       \
        reinterpret_cast<type_name *>(ptr)->~type_name();                     \
    }                                                                         \
    static ::rbc::world::TypeObjectRegister<type_name> type_name##_register_{ \
        ea525e13_create_##type_name,                                          \
        ea525e13_destroy_##type_name};                                        \
    void type_name::rbc_rc_delete() {                                         \
        type_name##_register_.destroy(this);                                  \
    }
}// namespace rbc::world