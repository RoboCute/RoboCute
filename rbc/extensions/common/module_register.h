#pragma once
#include <pybind11/pybind11.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/binary_io.h>
#include "guid.h"
#include <res_creation_info.h>
#include <luisa/core/spin_mutex.h>
namespace py = pybind11;
struct ModuleRegister {
private:
    static ModuleRegister *header;
    ModuleRegister *next;

public:
    static void init(py::module &m);
    void (*_callback)(py::module &);
    explicit ModuleRegister(void (*callback)(py::module &));
};
template<typename... Args>
luisa::move_only_function<void(Args...)> to_cppfunc_5d4636ab(py::function const &f) {
    return [f](Args... args) {
        f(static_cast<void*>(std::forward<Args>(args))...);
    };
}

template<typename T>
class raw_ptr {

private:
    T *_p;

public:
    [[nodiscard]] raw_ptr(T *p) noexcept : _p{p} {}
    [[nodiscard]] T *get() const noexcept { return _p; }
    [[nodiscard]] T *operator->() const noexcept { return _p; }
    [[nodiscard]] T &operator*() const noexcept { return *_p; }
    [[nodiscard]] explicit operator bool() const noexcept { return _p != nullptr; }
};

inline luisa::span<std::byte> to_span_5d4636ab(py::buffer const &b) {
    auto r = b.request();
    return {
        (std::byte *)r.ptr,
        (size_t)r.size * (size_t)r.itemsize};
}

inline py::memoryview to_memoryview_5d4636ab(luisa::span<std::byte> const &sp) {
    return py::memoryview::from_memory(
        sp.data(),
        sp.size());
}
template<typename T>
using Vec = luisa::vector<T>;