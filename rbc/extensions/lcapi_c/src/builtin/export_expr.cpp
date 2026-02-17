#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <luisa/ast/function_builder.h>
#include <luisa/runtime/dispatch_buffer.h>
#include "module_register.h"
namespace py = pybind11;
using namespace luisa;
using namespace luisa::compute;
constexpr auto pyref = py::return_value_policy::reference;
using luisa::compute::detail::FunctionBuilder;

template<typename T>
class raw_ptr {

private:
    T *_p;

public:
    [[nodiscard]] raw_ptr(T *p) noexcept : _p{p} {}
    [[nodiscard]] T *get() const noexcept { return _p; }
    [[nodiscard]] T *operator->() const noexcept { return _p; }
    [[nodiscard]] T &operator*() const noexcept { return *_p; }
    [[nodiscard]] explicit operator bool() const noexcept { return _p != nullptr; }
};

PYBIND11_DECLARE_HOLDER_TYPE(T, raw_ptr<T>, true)
void export_expr(py::module &m) {
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
        .def_static(
            "custom", [](luisa::string_view str) {
                return Type::custom(str);
            },
            pyref);
}

static ModuleRegister module_register_export_expr(export_expr);
