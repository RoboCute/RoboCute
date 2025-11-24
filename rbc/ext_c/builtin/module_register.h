#pragma once
#include <nanobind/nanobind.h>
#include <luisa/core/dynamic_module.h>

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

    static void init(nanobind::module_ &m);
    void (*_callback)(nanobind::module_ &);
    ModuleRegister(void (*callback)(nanobind::module_ &));
};
inline nanobind::str to_nb_str(auto const &str) {
    if constexpr (requires {str.data(); str.size(); }) {
        return nanobind::str{str.data(), str.size()};
    } else if constexpr (requires { str.c_str(); }) {
        return nanobind::str{str.c_str()};
    } else {
        return nanobind::str{str};
    }
}