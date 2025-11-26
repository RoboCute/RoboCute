#include "module_register.h"
namespace py = pybind11;
using namespace nb::literals;
NB_MODULE(rbc_ext_c, m) {
    ModuleRegister::init(m);
}