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
    ModuleRegister(void (*callback)(py::module &));
};