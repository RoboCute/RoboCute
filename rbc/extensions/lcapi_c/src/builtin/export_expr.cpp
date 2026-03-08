#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <luisa/ast/function_builder.h>
#include <luisa/runtime/dispatch_buffer.h>
#include <luisa/core/logging.h>
#include "module_register.h"
#include "arg_types.h"
namespace py = pybind11;
using namespace luisa;
using namespace luisa::compute;
constexpr auto pyref = py::return_value_policy::reference;

PYBIND11_DECLARE_HOLDER_TYPE(T, raw_ptr<T>, true)
void ArgTypes::check_same(ArgTypes const &other_) {
    if (types.size() != other_.types.size()) {
        LUISA_ERROR("ArgTypes size mismatch: {} vs {}", types.size(), other_.types.size());
    }
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i] != other_.types[i]) {
            LUISA_ERROR("ArgTypes element mismatch at index {}: {} vs {}", i, types[i]->description(), other_.types[i]->description());
        }
    }
}
void ArgTypes::append(Type const *ptr) {
    types.push_back(ptr);
}

void export_expr(py::module &m) {
    py::class_<ArgTypes>(m, "ArgTypes")
        .def(py::init<>())
        .def("check_same", &ArgTypes::check_same)
        .def("append", &ArgTypes::append)
        .def("get", &ArgTypes::get)
        .def("size", &ArgTypes::size);
    py::class_<Type, raw_ptr<Type>>(m, "Type")
        .def_static("from_", &Type::from, pyref)
        .def("size", &Type::size)
        .def("alignment", &Type::alignment)
        .def("is_scalar", &Type::is_scalar)
        .def("is_vector", &Type::is_vector)
        .def("is_matrix", &Type::is_matrix)
        .def("is_basic", &Type::is_basic)
        .def("is_array", &Type::is_array)
        .def("is_structure", &Type::is_structure)
        .def("is_buffer", &Type::is_buffer)
        .def("is_texture", &Type::is_texture)
        .def("is_bindless_array", &Type::is_bindless_array)
        .def("is_accel", &Type::is_accel)
        .def("is_custom", &Type::is_custom)
        .def("element", &Type::element, pyref)
        .def("description", &Type::description)
        .def("dimension", &Type::dimension)
        .def("is_custom_buffer", [](Type const *t) {
            return t == Type::of<IndirectDispatchBuffer>();
        })
        .def_static("custom", [](luisa::string_view str) { return Type::custom(str); }, pyref);
}

static ModuleRegister module_register_export_expr(export_expr);
