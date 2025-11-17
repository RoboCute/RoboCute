#include "module_register.h"
namespace nb = nanobind;
using namespace nb::literals;
NB_MODULE(test_py_codegen, m){
    ModuleRegister::init(m);
}
