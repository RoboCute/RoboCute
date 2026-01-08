#include "module_register.h"
namespace py = pybind11;
PYBIND11_MODULE(test_py_codegen, m) {
    ModuleRegister::init(m);
}
