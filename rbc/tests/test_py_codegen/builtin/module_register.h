#pragma once
#include <pybind11/pybind11.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/binary_io.h>
#include "guid.h"
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