#include <pybind11/pybind11.h>
#include <luisa/core/logging.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/basic_types.h>
#include "module_register.h"
#include <rbc_core/rc.h>
#include <rbc_world/base_object.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/runtime_static.h>
namespace py = pybind11;
using namespace luisa;
using namespace rbc;
struct Disposer {
    bool _disposed{false};
    void dispose() {
        if (_disposed) return;
        _disposed = true;
        LUISA_INFO("RBC disposed.");
        rbc::world::destroy_world();
        rbc::PluginManager::destroy_instance();
        rbc::RuntimeStaticBase::dispose_all();
    }
    ~Disposer() {
        dispose();
    }
};
static Disposer _disposer;
void export_base_obj(py::module &m) {
    m.def("rbc_add_ref", [](void *ptr) {
        manually_add_ref(static_cast<RCBase *>(ptr));
    });
    m.def("rbc_release", [](void *ptr) {
        manually_release_ref(static_cast<RCBase *>(ptr));
    });
    m.def("_create_resource", [&](luisa::string_view type_info) -> void * {
        vstd::MD5 md5{type_info};
        rbc::TypeInfo type{type_info, md5};
        auto ptr = world::create_object(type);
        if (!ptr) return nullptr;
        if (ptr->base_type() != world::BaseObjectType::Resource) [[unlikely]] {
            LUISA_ERROR("Trying to create a non-resource object.");
        }
    });
    m.def("_create_resource_guid", [&](luisa::string_view type_info, GuidData guid) -> void * {
        vstd::MD5 md5{type_info};
        rbc::TypeInfo type{type_info, md5};
        auto ptr = world::create_object_with_guid(type, reinterpret_cast<vstd::Guid const &>(guid));
        if (!ptr) return nullptr;
        if (ptr->base_type() != world::BaseObjectType::Resource) [[unlikely]] {
            LUISA_ERROR("Trying to create a non-resource object.");
        }
    });
}

static ModuleRegister module_register_export_base_obj(export_base_obj);