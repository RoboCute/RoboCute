#include <pybind11/pybind11.h>
#include <luisa/core/logging.h>
#include <luisa/core/mathematics.h>
#include "module_register.h"
#include <rbc_core/rc.h>

namespace py = pybind11;
using namespace luisa;

void export_memory(py::module &m) {
    m.def("destroy_object", [](void *ptr) {
        manually_release_ref((rbc::RCBase *)ptr);
    });
}
static ModuleRegister module_register_export_memory(export_memory);
