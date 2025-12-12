#pragma once
#include <rbc_world_v2/base_object.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/pool.h>
namespace rbc::world {
RBC_WORLD_API BaseObject *get_object(InstanceID instance_id);
RBC_WORLD_API BaseObject *get_object(vstd::Guid const &guid);
struct TypeRegisterBase;
using CreateFunc = vstd::func_ptr_t<BaseObject *()>;
struct TypeRegisterBase {
    BaseObjectType base_obj_type;
    TypeRegisterBase *p_next{};
    RBC_WORLD_API TypeRegisterBase();
    ~TypeRegisterBase() = default;
    virtual std::array<uint64_t, 2> type_id() = 0;
    virtual BaseObject *create() = 0;
    virtual void init() = 0;
    virtual void destroy() = 0;
};
template<typename T>
    requires std::is_base_of_v<BaseObject, T>
struct TypeRegister : TypeRegisterBase {
    vstd::spin_mutex _mtx;
    vstd::optional<vstd::Pool<vstd::Storage<T>, true>> _pool;
    vstd::func_ptr_t<void(T *)> _ctor_func;
    vstd::func_ptr_t<void(T *)> _dtor_func;
    vstd::func_ptr_t<void()> _on_init;
    vstd::func_ptr_t<void()> _on_destroy;
    TypeRegister(
        vstd::func_ptr_t<void(T *)> ctor_func,
        vstd::func_ptr_t<void(T *)> dtor_func,
        vstd::func_ptr_t<void()> on_init = nullptr,
        vstd::func_ptr_t<void()> on_destroy = nullptr)
        : _ctor_func(ctor_func),
          _dtor_func(dtor_func),
          _on_init(on_init),
          _on_destroy(on_destroy) {
        TypeRegisterBase::base_obj_type = T::base_object_type_v;
    }
    void init() override {
        _pool.create(64, false);
        if (_on_init)
            _on_init();
    }
    void destroy() override {
        _pool.destroy();
        if (_on_destroy)
            _on_destroy();
    }
    std::array<uint64_t, 2> type_id() override {
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

#define DECLARE_WORLD_TYPE_REGISTER(type_name, ...)       \
    void _create_##type_name(type_name *ptr) {            \
        new (std::launder(ptr)) type_name{};              \
    }                                                     \
    void _destroy_##type_name(type_name *ptr) {           \
        reinterpret_cast<type_name *>(ptr)->~type_name(); \
    }                                                     \
    static TypeRegister<type_name> type_name##_register_{ \
        _create_##type_name,                              \
        _destroy_##type_name,                             \
        __VA_ARGS__};                                     \
    void type_name::dispose() {                           \
        type_name##_register_.destroy(this);              \
    }
}// namespace rbc::world