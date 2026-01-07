#include "module_register.h"
#include <rbc_core/runtime_static.h>
#include <rbc_world/base_object.h>
#include <rbc_plugin/plugin_manager.h>
namespace py = pybind11;
struct Initializer {
    Initializer() {
        rbc::RuntimeStaticBase::init_all();
        rbc::PluginManager::init();
    }
    ~Initializer() {
        rbc::world::destroy_world();
        rbc::PluginManager::destroy_instance();
        rbc::RuntimeStaticBase::dispose_all();
    }
};
static vstd::optional<Initializer> _initializer;
PYBIND11_MODULE(test_py_codegen, m) {
    _initializer.create();
    ModuleRegister::init(m);
    m.def("init_world", [](luisa::string_view meta_path, luisa::string_view binary_path) {
        rbc::world::init_world(
            meta_path, binary_path);
    });
}
