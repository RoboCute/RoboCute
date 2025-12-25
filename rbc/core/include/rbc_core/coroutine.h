#pragma once
#include <rbc_config.h>
#include <coroutine>
#include <memory>
#include <luisa/core/stl/type_traits.h>

namespace rbc {
struct promise;
// Coro from cppref
struct coroutine {
    friend struct promise;
    using promise_type = rbc::promise;
    using base_type = std::coroutine_handle<rbc::promise>;
private:
    base_type _base;
    bool _own{};
public:
    coroutine() = default;
    coroutine(base_type &&rhs) : _base(rhs), _own{true} {}
    coroutine(coroutine const &) = delete;
    RBC_CORE_API coroutine(coroutine &&rhs);
    static auto from_promise(promise &prom) {
        return base_type::from_promise(prom);
    }
    coroutine &operator=(coroutine const &rhs) = delete;
    coroutine &operator=(coroutine &&rhs) {
        std::destroy_at(this);
        std::construct_at(this, std::move(rhs));
        return *this;
    }
    RBC_CORE_API void resume();
    RBC_CORE_API bool done();
    RBC_CORE_API void destroy();
    RBC_CORE_API ~coroutine();
};
struct i_awaitable;
struct promise {
    i_awaitable *_awaitable{};
    promise() {
    }
    ~promise() {
    }
    coroutine get_return_object() {
        return {coroutine::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }
    void return_void() {
    }
    void unhandled_exception() {}
};
struct i_awaitable {
private:
    promise *_promise{};
public:
    virtual bool await_ready() = 0;
    i_awaitable() = default;
    i_awaitable(i_awaitable const &) = delete;
    i_awaitable(i_awaitable &&) = delete;
    void await_suspend(std::coroutine_handle<rbc::promise> h) {
        _promise = &h.promise();
        _promise->_awaitable = this;
    }
    void await_resume() {}
protected:
    ~i_awaitable() = default;
};
template<typename FuncType>
    requires((std::is_invocable_r_v<bool, FuncType>) && (!std::is_reference_v<FuncType>))
struct awaitable final : i_awaitable {
    FuncType func_type;
    template<typename... Args>
        requires(luisa::is_constructible_v<FuncType, Args && ...>)
    awaitable(Args &&...args)
        : func_type(std::forward<Args>(args)...) {}
    bool await_ready() override {
        return func_type();
    }
};
}// namespace rbc