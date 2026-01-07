#include <pybind11/pybind11.h>
#include <luisa/core/logging.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/basic_types.h>
#include "module_register.h"
#include <rbc_core/rc.h>
#include <rbc_world/base_object.h>
namespace py = pybind11;
using namespace luisa;
using namespace rbc;
void export_base_obj(py::module &m) {
    m.def("_create_resource", [&](luisa::string_view type_info) -> void * {
        vstd::MD5 md5{type_info};
        rbc::TypeInfo type{type_info, md5.to_binary().data0, md5.to_binary().data1};
        auto ptr = world::create_object(type);
        if (!ptr) return nullptr;
        if (ptr->base_type() != world::BaseObjectType::Resource) [[unlikely]] {
            LUISA_ERROR("Trying to create a non-resource object.");
        }
    });
    m.def("_create_resource_guid", [&](luisa::string_view type_info, GuidData guid) -> void * {
        vstd::MD5 md5{type_info};
        rbc::TypeInfo type{type_info, md5.to_binary().data0, md5.to_binary().data1};
        auto ptr = world::create_object_with_guid(type, reinterpret_cast<vstd::Guid const &>(guid));
        if (!ptr) return nullptr;
        if (ptr->base_type() != world::BaseObjectType::Resource) [[unlikely]] {
            LUISA_ERROR("Trying to create a non-resource object.");
        }
    });
}

static ModuleRegister module_register_export_base_obj(export_base_obj);