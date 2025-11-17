#include "module_register.h"
ModuleRegister* ModuleRegister::header{};
ModuleRegister::ModuleRegister(void (*callback)(nanobind::module_&))
    : _callback(callback)
{
    next = header;
    header = this;
}
void ModuleRegister::init(nanobind::module_& m)
{
    auto ptr = header;
    while (ptr)
    {
        ptr->_callback(m);
        ptr = ptr->next;
    }
}