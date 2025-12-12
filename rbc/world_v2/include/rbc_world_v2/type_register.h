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
    friend struct BaseObjectStatics;
    virtual BaseObject *create() = 0;
private:
    TypeRegisterBase *p_next{};
    virtual std::array<uint64_t, 2> type_id() = 0;
    virtual void init() = 0;
    virtual void destroy() = 0;
protected:
    BaseObjectType base_obj_type;
    RBC_WORLD_API TypeRegisterBase();
    ~TypeRegisterBase() = default;
public:
    BaseObjectType base_type() const { return base_obj_type; }
};
struct RuntimeStaticBase {
    friend struct BaseObjectStatics;
protected:
    RuntimeStaticBase *p_next{};
    RBC_WORLD_API RuntimeStaticBase();
    RBC_WORLD_API ~RuntimeStaticBase();
private:
    virtual void init() = 0;
    virtual void destroy() = 0;
};
template<typename T, typename... Args>
    requires luisa::is_constructible_v<T, Args...>
struct RuntimeStatic : RuntimeStaticBase {
    vstd::optional<T> ptr;
    std::tuple<Args...> _args;
    RuntimeStatic(Args &&...args)
        : _args(std::forward<Args>(args)...) {}
    T const *operator->() const {
        return ptr.ptr();
    }
    T *operator->() {
        return ptr.ptr();
    }
    T const &operator*() const {
        return *ptr;
    }
    T &operator*() {
        return *ptr;
    }
protected:
    void init() override {
        std::apply(
            [&](Args &&...args) {
                ptr.create(std::forward<Args>(args)...);
            },
            std::move(_args));
    }
    void destroy() override {
        ptr.destroy();
    }
};
template<typename T>
    requires std::is_default_constructible_v<T>
struct RuntimeStatic<T> : RuntimeStaticBase {
    vstd::optional<T> ptr;
    T const *operator->() const {
        return ptr.ptr();
    }
    T *operator->() {
        return ptr.ptr();
    }
    T const &operator*() const {
        return *ptr;
    }
    T &operator*() {
        return *ptr;
    }
protected:
    void init() override {
        ptr.create();
    }
    void destroy() override {
        ptr.destroy();
    }
};
template<typename T>
    requires std::is_base_of_v<BaseObject, T>
struct TypeRegister : TypeRegisterBase {
    vstd::spin_mutex _mtx;
    vstd::optional<vstd::Pool<vstd::Storage<T>, true>> _pool;
    vstd::func_ptr_t<void(T *)> _ctor_func;
    vstd::func_ptr_t<void(T *)> _dtor_func;
    TypeRegister(
        vstd::func_ptr_t<void(T *)> ctor_func,
        vstd::func_ptr_t<void(T *)> dtor_func)
        : _ctor_func(ctor_func),
          _dtor_func(dtor_func) {
        TypeRegisterBase::base_obj_type = T::base_object_type_v;
    }
    void init() override {
        _pool.create(64, false);
    }
    void destroy() override {
        _pool.destroy();
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

#define DECLARE_WORLD_TYPE_REGISTER(type_name)            \
    void _create_##type_name(type_name *ptr) {            \
        new (std::launder(ptr)) type_name{};              \
    }                                                     \
    void _destroy_##type_name(type_name *ptr) {           \
        reinterpret_cast<type_name *>(ptr)->~type_name(); \
    }                                                     \
    static TypeRegister<type_name> type_name##_register_{ \
        _create_##type_name,                              \
        _destroy_##type_name};                            \
    void type_name::dispose() {                           \
        type_name##_register_.destroy(this);              \
    }
}// namespace rbc::world