#pragma once
#include <rbc_config.h>
#include <coroutine>
#include <luisa/vstl/meta_lib.h>

namespace rbc::coro {
struct promise;
// Coro from cppref
struct coroutine : std::coroutine_handle<rbc::coro::promise> {
    using promise_type = rbc::coro::promise;
};

struct promise {
    coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};

struct awaitable {
    bool _ready;
    awaitable(bool ready = false) : _ready{ready} {}
    bool await_ready() { return _ready; }
    void await_suspend(std::coroutine_handle<> h) {
    }
    void await_resume() {}
};
// generator from cppref
RBC_CORE_API void throw_coro_exception();
template<typename T>
struct generator {
    // The class name 'generator' is our choice and it is not required for coroutine
    // magic. Compiler recognizes coroutine by the presence of 'co_yield' keyword.
    // You can use name 'MyGenerator' (or any other name) instead as long as you include
    // nested struct promise_type with 'MyGenerator get_return_object()' method.
    // (Note: It is necessary to adjust the declarations of constructors and destructors
    //  when renaming.)

    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type// required
    {
        vstd::optional<T> value_{};

        generator get_return_object() {
            return generator(handle_type::from_promise(*this));
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() {
            throw_coro_exception();
        }// saving
        // exception

        template<std::convertible_to<T> From>// C++20 concept
        std::suspend_always yield_value(From &&from) {
            value_ = std::forward<From>(from);// caching the result in promise
            return {};
        }
        void return_void() {}
    };

    handle_type h_;

    generator(handle_type h) : h_(h) {}
    ~generator() { h_.destroy(); }
    vstd::optional<T> operator()() {
        fill();
        full_ = false;// we are going to move out previously cached
        // result to make promise empty again
        auto &&v = h_.promise().value_;
        if (!v) return {};
        auto dsp = vstd::scope_exit([&] {
            v.destroy();
        });
        return {std::move(*v)};
    }
    [[nodiscard]] void *address() const {
        return h_.address();
    }
    void destroy() const {
        h_.destroy();
    }
    [[nodiscard]] bool done() {
        fill();// The only way to reliably find out whether or not we finished coroutine,
               // whether or not there is going to be a next value generated (co_yield)
               // in coroutine via C++ getter (operator () below) is to execute/resume
               // coroutine until the next co_yield point (or let it fall off end).
               // Then we store/cache result in promise to allow getter (operator() below
               // to grab it without executing coroutine).
        return h_.done();
    }
    void resume() const {
        h_.resume();
    }
private:
    bool full_ = false;

    void fill() {
        if (!full_) {
            h_();
            full_ = true;
        }
    }
};

}// namespace rbc::coro