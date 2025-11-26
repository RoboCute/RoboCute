#pragma once
#include <pybind11/pybind11.h>
#include <luisa/core/dynamic_module.h>
#include "guid.h"
namespace py = pybind11;
struct ModuleRegister {
private:
    static ModuleRegister *header;
    ModuleRegister *next;

public:
    struct RefCountModule {
        luisa::string name;
        luisa::DynamicModule dll;
        std::atomic_uint64_t rc{};
        std::mutex load_mtx;
    };
    static RefCountModule *load_module(std::string_view name);
    static void module_addref(
        char const *folder,
        RefCountModule &module);
    static void module_deref(RefCountModule &module);

    static void init(py::module &m);
    void (*_callback)(py::module &);
    ModuleRegister(void (*callback)(py::module &));
};
inline luisa::string to_nb_str(auto const &str) {
    if constexpr (requires {str.data(); str.size(); }) {
        return luisa::string{str.data(), str.size()};
    } else if constexpr (requires { str.c_str(); }) {
        return luisa::string{str.c_str()};
    } else {
        return luisa::string{str};
    }
}