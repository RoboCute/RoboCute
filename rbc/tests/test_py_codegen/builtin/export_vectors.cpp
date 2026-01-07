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
        auto from_vector = [](auto const &rc) { return rc.get(); };
        py::class_<Vec<RC<RCBase>>>(m, "capsule_vector")
            .def(py::init<>())
            .def("clear", [&](Vec<RC<RCBase>> &vec) {
                vec.vec.clear();
            })
            .def("__len__", [&](Vec<RC<RCBase>> &vec) {
                return vec.vec.size();
            })
            .def_property_readonly("capacity", [&](Vec<RC<RCBase>> &vec) {
                return vec.vec.capacity();
            })
            .def_property_readonly("empty", [&](Vec<RC<RCBase>> &vec) {
                return vec.vec.empty();
            })
            .def("reserve", [&](Vec<RC<RCBase>> &vec, uint64_t capacity) {
                vec.vec.reserve(capacity);
            })
            .def("emplace_back", [&](Vec<RC<RCBase>> &vec, void *ptr) {
                vec.vec.emplace_back((RCBase *)(ptr));
            })
            .def("pop_back", [&](Vec<RC<RCBase>> &vec) {
                if (vec.vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call pop_back() in empty vector.");
                }
                vec.vec.pop_back();
            })
            .def("back", [&](Vec<RC<RCBase>> &vec) -> void * {
                if (vec.vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call back() in empty vector.");
                }
                return from_vector(vec.vec.back());
            })
            .def("__getitem__", [&](Vec<RC<RCBase>> &vec, uint64_t idx) -> void * {
                if (idx >= vec.vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return from_vector(vec.vec[idx]);
            })
            .def("__setitem__", [&](Vec<RC<RCBase>> &vec, uint64_t idx, void *ptr) {
                if (idx >= vec.vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return vec.vec[idx] = (RCBase *)(ptr);
            });

        py::class_<Vec<int>>(m, "int_vector")
            .def(py::init<>())
            .def("clear", [&](Vec<int> &vec) {
                vec.vec.clear();
            })
            .def("__len__", [&](Vec<int> &vec) {
                return vec.vec.size();
            })
            .def_property_readonly("capacity", [&](Vec<int> &vec) {
                return vec.vec.capacity();
            })
            .def_property_readonly("empty", [&](Vec<int> &vec) {
                return vec.vec.empty();
            })
            .def("reserve", [&](Vec<int> &vec, uint64_t capacity) {
                vec.vec.reserve(capacity);
            })
            .def("emplace_back", [&](Vec<int> &vec, int ptr) {
                vec.vec.emplace_back(ptr);
            })
            .def("pop_back", [&](Vec<int> &vec) {
                if (vec.vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call pop_back() in empty vector.");
                }
                vec.vec.pop_back();
            })
            .def("back", [&](Vec<int> &vec) -> int {
                if (vec.vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call back() in empty vector.");
                }
                return vec.vec.back();
            })
            .def("__getitem__", [&](Vec<int> &vec, uint64_t idx) -> int {
                if (idx >= vec.vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return vec.vec[idx];
            })
            .def("__setitem__", [&](Vec<int> &vec, uint64_t idx, int ptr) {
                if (idx >= vec.vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return vec.vec[idx] = ptr;
            });
    }
}
static ModuleRegister module_register_export_vectors(export_vectors);