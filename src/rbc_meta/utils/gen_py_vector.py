from rbc_meta.utils.codegen_util import *
from pathlib import Path
root_path = Path(__file__).parent.parent.parent.parent / 'rbc/tests/test_py_codegen/builtin'
template_str = '''#include <pybind11/pybind11.h>
#include <luisa/core/logging.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/basic_types.h>
#include "module_register.h"
#include <rbc_core/rc.h>
namespace py = pybind11;
using namespace luisa;
using namespace rbc;
void export_vectors(py::module &m) {
#IMPL#
}
static ModuleRegister module_register_export_vectors(export_vectors);'''
vector_template_str = '''    {
    #BEFORE_DECL#
    py::class_<luisa::vector<#ELEM_TYPE#>>(m, "#NAME#")
        .def(py::init<>())
        .def("clear", [&](luisa::vector<#ELEM_TYPE#> &vec) {
            vec.clear();
        })
        .def("__len__", [&](luisa::vector<#ELEM_TYPE#> &vec) {
            return vec.size();
        })
        .def_property_readonly("capacity", [&](luisa::vector<#ELEM_TYPE#> &vec) {
            return vec.capacity();
        })
        .def_property_readonly("empty", [&](luisa::vector<#ELEM_TYPE#> &vec) {
            return vec.empty();
        })
        .def("reserve", [&](luisa::vector<#ELEM_TYPE#> &vec, uint64_t capacity) {
            vec.reserve(capacity);
        })
        .def("emplace_back", [&](luisa::vector<#ELEM_TYPE#> &vec, #PY_ELEM_TYPE# ptr) {
            vec.emplace_back(#TO_VECTOR#(ptr));
        })
        .def("pop_back", [&](luisa::vector<#ELEM_TYPE#> &vec) {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call pop_back() in empty vector.");
            }
            vec.pop_back();
        })
        .def("back", [&](luisa::vector<#ELEM_TYPE#> &vec) -> #PY_ELEM_TYPE# {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call back() in empty vector.");
            }
            return #FROM_VECTOR#(vec.back());
        })
        .def("__getitem__", [&](luisa::vector<#ELEM_TYPE#> &vec, uint64_t idx) -> #PY_ELEM_TYPE# {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("Index out of range");
            }
            return #FROM_VECTOR#(vec[idx]);
        })
        .def("__setitem__", [&](luisa::vector<#ELEM_TYPE#> &vec, uint64_t idx, #PY_ELEM_TYPE# ptr) {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("Index out of range");
            }
            return vec[idx] = #TO_VECTOR#(ptr);
        });
    }
'''

def to_vec_str(table: dict):
    real_table = dict()
    for i in table:
        real_table['#' + i + '#'] = table[i]
    return re_translate(vector_template_str, real_table)
result = ''
# handle
replace_table = {
    'NAME': 'capsule_vector',
    'ELEM_TYPE': 'RC<RCBase>',
    'PY_ELEM_TYPE': 'void*',
    'BEFORE_DECL': 'auto from_vector = [](auto const& rc){ return rc.get(); }; ',
    'TO_VECTOR': '(RCBase*)',
    'FROM_VECTOR':  'from_vector',
}

result += to_vec_str(replace_table)
# string
replace_table = {
    'NAME': 'string_vector',
    'ELEM_TYPE': 'luisa::string',
    'PY_ELEM_TYPE': 'char const*',
    'BEFORE_DECL': 'auto from_vector = [](auto const& s) { return s.c_str(); };',
    'TO_VECTOR': '',
    'FROM_VECTOR':  'from_vector',
} 

result += to_vec_str(replace_table)
codegen_to(root_path / 'export_vectors.cpp')(lambda: template_str.replace('#IMPL#', result))