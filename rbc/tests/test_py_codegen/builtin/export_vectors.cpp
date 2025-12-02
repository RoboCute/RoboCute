#include <pybind11/pybind11.h>
#include <luisa/core/logging.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/basic_types.h>
#include "module_register.h"
#include <rbc_core/rc.h>
namespace py = pybind11;
using namespace luisa;
using namespace rbc;
void export_vectors(py::module &m) {
    {
    auto from_vector = [](auto const& rc){ return rc.get(); }; 
    py::class_<luisa::vector<RC<RCBase>>>(m, "capsule_vector")
        .def(py::init<>())
        .def("clear", [&](luisa::vector<RC<RCBase>> &vec) {
            vec.clear();
        })
        .def("__len__", [&](luisa::vector<RC<RCBase>> &vec) {
            return vec.size();
        })
        .def_property_readonly("capacity", [&](luisa::vector<RC<RCBase>> &vec) {
            return vec.capacity();
        })
        .def_property_readonly("empty", [&](luisa::vector<RC<RCBase>> &vec) {
            return vec.empty();
        })
        .def("reserve", [&](luisa::vector<RC<RCBase>> &vec, uint64_t capacity) {
            vec.reserve(capacity);
        })
        .def("emplace_back", [&](luisa::vector<RC<RCBase>> &vec, void* ptr) {
            vec.emplace_back((RCBase*)(ptr));
        })
        .def("pop_back", [&](luisa::vector<RC<RCBase>> &vec) {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call pop_back() in empty vector.");
            }
            vec.pop_back();
        })
        .def("back", [&](luisa::vector<RC<RCBase>> &vec) -> void* {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call back() in empty vector.");
            }
            return from_vector(vec.back());
        })
        .def("__getitem__", [&](luisa::vector<RC<RCBase>> &vec, uint64_t idx) -> void* {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("idnex out of range");
            }
            return from_vector(vec[idx]);
        })
        .def("__setitem__", [&](luisa::vector<RC<RCBase>> &vec, uint64_t idx, void* ptr) {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("idnex out of range");
            }
            return vec[idx] = (RCBase*)(ptr);
        });
    }
    {
    auto from_vector = [](auto const& s) { return s.c_str(); };
    py::class_<luisa::vector<luisa::string>>(m, "string_vector")
        .def(py::init<>())
        .def("clear", [&](luisa::vector<luisa::string> &vec) {
            vec.clear();
        })
        .def("__len__", [&](luisa::vector<luisa::string> &vec) {
            return vec.size();
        })
        .def_property_readonly("capacity", [&](luisa::vector<luisa::string> &vec) {
            return vec.capacity();
        })
        .def_property_readonly("empty", [&](luisa::vector<luisa::string> &vec) {
            return vec.empty();
        })
        .def("reserve", [&](luisa::vector<luisa::string> &vec, uint64_t capacity) {
            vec.reserve(capacity);
        })
        .def("emplace_back", [&](luisa::vector<luisa::string> &vec, char const* ptr) {
            vec.emplace_back((ptr));
        })
        .def("pop_back", [&](luisa::vector<luisa::string> &vec) {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call pop_back() in empty vector.");
            }
            vec.pop_back();
        })
        .def("back", [&](luisa::vector<luisa::string> &vec) -> char const* {
            if (vec.empty()) [[unlikely]] {
                LUISA_ERROR("call back() in empty vector.");
            }
            return from_vector(vec.back());
        })
        .def("__getitem__", [&](luisa::vector<luisa::string> &vec, uint64_t idx) -> char const* {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("idnex out of range");
            }
            return from_vector(vec[idx]);
        })
        .def("__setitem__", [&](luisa::vector<luisa::string> &vec, uint64_t idx, char const* ptr) {
            if (idx >= vec.size()) [[unlikely]] {
                LUISA_ERROR("idnex out of range");
            }
            return vec[idx] = (ptr);
        });
    }

}
static ModuleRegister module_register_export_vectors(export_vectors);