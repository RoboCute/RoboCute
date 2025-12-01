#pragma once
#include <pybind11/pybind11.h>
#include <luisa/core/dynamic_module.h>
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
