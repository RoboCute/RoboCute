#include "builtin/module_register.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void register_resource_bindings(py::module &m) {
}

static ModuleRegister module_register_register_resource_bindings(register_resource_bindings);