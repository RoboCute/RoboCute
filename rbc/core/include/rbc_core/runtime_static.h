#pragma once
#include <luisa/vstl/common.h>
#include <rbc_config.h>
namespace rbc {
struct RuntimeStaticBase {
    friend struct BaseObjectStatics;
    RBC_CORE_API static void init_all();
    RBC_CORE_API static void dispose_all();
protected:
    RuntimeStaticBase *_p_next{};
    RuntimeStaticBase() = default;
    ~RuntimeStaticBase() = default;
    RBC_CORE_API void _base_init();
    static RBC_CORE_API void _check_ptr(bool ptr);
private:
    virtual void _init() = 0;
    virtual void _destroy() = 0;
};
template<typename T, typename... Args>
    requires luisa::is_constructible_v<T, Args...>
struct RuntimeStatic : RuntimeStaticBase {
    vstd::optional<T> ptr;
    std::tuple<Args...> _args;
    RuntimeStatic(Args &&...args)
        : _args(std::forward<Args>(args)...) {
        _base_init();
    }
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

    explicit operator bool() const {
        return ptr.has_value();
    }
protected:
    void _init() override {
        std::apply(
            [&](Args &&...args) {
                ptr.create(std::forward<Args>(args)...);
            },
            std::move(_args));
    }
    void _destroy() override {
        ptr.destroy();
    }
};
template<typename T>
    requires std::is_default_constructible_v<T>
struct RuntimeStatic<T> : RuntimeStaticBase {
    vstd::optional<T> ptr;
    RuntimeStatic() {
        _base_init();
    }
    T const *operator->() const {
#ifndef NDEBUG
        RuntimeStaticBase::_check_ptr(ptr);
#endif
        return ptr.ptr();
    }
    T *operator->() {
#ifndef NDEBUG
        RuntimeStaticBase::_check_ptr(ptr);
#endif
        return ptr.ptr();
    }
    operator bool() const {
        return ptr.has_value();
    }
protected:
    void _init() override {
        ptr.create();
    }
    void _destroy() override {
        ptr.destroy();
    }
};
}// namespace rbc