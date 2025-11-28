#include "module_register.h"
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/string.h>
ModuleRegister *ModuleRegister::header{};
ModuleRegister::ModuleRegister(void (*callback)(py::module &))
    : _callback(callback) {
    next = header;
    header = this;
}
void ModuleRegister::init(py::module &m) {
    auto ptr = header;
    while (ptr) {
        ptr->_callback(m);
        ptr = ptr->next;
    }
}
