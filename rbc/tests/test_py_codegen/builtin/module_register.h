#pragma once
#include <nanobind/nanobind.h>

struct ModuleRegister {
private:
    static ModuleRegister* header;
    ModuleRegister* next;

public:
    static void init(nanobind::module_& m);
    void (*_callback)(nanobind::module_&);
    ModuleRegister(void (*callback)(nanobind::module_&));
};
inline nanobind::str to_nb_str(auto const& str)
{
    if constexpr (requires {str.data(); str.size(); })
    {
        return nanobind::str{ str.data(), str.size() };
    }
    else if constexpr (requires { str.c_str(); })
    {
        return nanobind::str{ str.c_str() };
    }
    else
    {
        return nanobind::str{ str };
    }
}