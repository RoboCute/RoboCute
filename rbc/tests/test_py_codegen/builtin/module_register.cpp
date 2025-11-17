#include "module_register.h"
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/string.h>
ModuleRegister *ModuleRegister::header{};
ModuleRegister::ModuleRegister(void (*callback)(nanobind::module_ &))
    : _callback(callback) {
    next = header;
    header = this;
}
void ModuleRegister::init(nanobind::module_ &m) {
    auto ptr = header;
    while (ptr) {
        ptr->_callback(m);
        ptr = ptr->next;
    }
}
static std::mutex module_ref_mtx;
static luisa::unordered_map<luisa::string, ModuleRegister::RefCountModule*> modules;

auto ModuleRegister::load_module(
    std::string_view name) -> RefCountModule * {
    module_ref_mtx.lock();
    auto iter = modules.try_emplace(name);
    auto &v = iter.first->second;
    if (!v) {
        v = new ModuleRegister::RefCountModule();
    }
    v->name = name;
    module_ref_mtx.unlock();
    return v;
}
void ModuleRegister::module_addref(
    const luisa::filesystem::path &folder,
    RefCountModule &module) {
    if (module.rc++ == 0) {
        std::lock_guard lck{module.load_mtx};
        if (!module.dll) {
            module.dll = luisa::DynamicModule::load(folder, module.name);
        }
    }
}
void ModuleRegister::module_deref(RefCountModule &module) {
    if (--module.rc == 0) {
        std::lock_guard lck{module.load_mtx};
        if (module.dll) {
            module.dll.reset();
        }
    }
}
