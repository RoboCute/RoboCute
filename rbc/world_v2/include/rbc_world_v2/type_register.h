#pragma once
#include <rbc_world_v2/component.h>
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
    virtual Component *create_component(Entity *) = 0;
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
    [[nodiscard]] BaseObjectType base_type() const { return base_obj_type; }
};

template<typename T>
    requires std::is_base_of_v<BaseObject, T> && (!std::is_base_of_v<Component, T>)
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
    Component *create_component(Entity *) override {
        return nullptr;
    }
    void destroy(T *ptr) {
        _dtor_func(ptr);
        _pool->destroy_lock(_mtx, reinterpret_cast<vstd::Storage<T> *>(ptr));
    }
};
template<typename T>
    requires std::is_base_of_v<Component, T>
struct TypeComponentRegister : TypeRegisterBase {
    vstd::spin_mutex _mtx;
    vstd::optional<vstd::Pool<vstd::Storage<T>, true>> _pool;
    vstd::func_ptr_t<void(T *, Entity *)> _ctor_func;
    vstd::func_ptr_t<void(T *)> _dtor_func;
    TypeComponentRegister(
        vstd::func_ptr_t<void(T *, Entity *)> ctor_func,
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
        return nullptr;
    }
    Component *create_component(Entity *entity) override {
        auto ptr = reinterpret_cast<T *>(_pool->create_lock(_mtx));
        _ctor_func(ptr, entity);
        return static_cast<Component *>(ptr);
    }
    void destroy(T *ptr) {
        _dtor_func(ptr);
        _pool->destroy_lock(_mtx, reinterpret_cast<vstd::Storage<T> *>(ptr));
    }
};

#define DECLARE_WORLD_OBJECT_REGISTER(type_name)                \
    void ea525e13_create_##type_name(type_name *ptr) {          \
        new (std::launder(ptr)) type_name{};                    \
    }                                                           \
    void ea525e13_destroy_##type_name(type_name *ptr) {         \
        reinterpret_cast<type_name *>(ptr)->~type_name();       \
    }                                                           \
    static TypeObjectRegister<type_name> type_name##_register_{ \
        ea525e13_create_##type_name,                            \
        ea525e13_destroy_##type_name};                          \
    void type_name::dispose() {                                 \
        type_name##_register_.destroy(this);                    \
    }
#define DECLARE_WORLD_COMPONENT_REGISTER(type_name)                \
    void ea525e13_create_##type_name(type_name *ptr, Entity *e) {  \
        new (std::launder(ptr)) type_name{e};                      \
    }                                                              \
    void ea525e13_destroy_##type_name(type_name *ptr) {            \
        reinterpret_cast<type_name *>(ptr)->~type_name();          \
    }                                                              \
    static TypeComponentRegister<type_name> type_name##_register_{ \
        ea525e13_create_##type_name,                               \
        ea525e13_destroy_##type_name};                             \
    void type_name::dispose() {                                    \
        type_name##_register_.destroy(this);                       \
    }
}// namespace rbc::world