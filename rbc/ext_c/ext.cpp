#include "module_register.h"

namespace py = pybind11;
using namespace pybind11::literals;
PYBIND11_MODULE(rbc_ext_c, m) {
    ModuleRegister::init(m);
}