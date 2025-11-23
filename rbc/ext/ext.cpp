#include "module_register.h"
namespace nb = nanobind;
using namespace nb::literals;
NB_MODULE(rbc_ext, m) {
    ModuleRegister::init(m);
}