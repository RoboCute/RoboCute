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
                vec.clear();
            })
            .def("__len__", [&](Vec<RC<RCBase>> &vec) {
                return vec.size();
            })
            .def_property_readonly("capacity", [&](Vec<RC<RCBase>> &vec) {
                return vec.capacity();
            })
            .def_property_readonly("empty", [&](Vec<RC<RCBase>> &vec) {
                return vec.empty();
            })
            .def("reserve", [&](Vec<RC<RCBase>> &vec, uint64_t capacity) {
                vec.reserve(capacity);
            })
            .def("emplace_back", [&](Vec<RC<RCBase>> &vec, void *ptr) {
                vec.emplace_back((RCBase *)(ptr));
            })
            .def("pop_back", [&](Vec<RC<RCBase>> &vec) {
                if (vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call pop_back() in empty vector.");
                }
                vec.pop_back();
            })
            .def("back", [&](Vec<RC<RCBase>> &vec) -> void * {
                if (vec.empty()) [[unlikely]] {
                    LUISA_ERROR("call back() in empty vector.");
                }
                return from_vector(vec.back());
            })
            .def("__getitem__", [&](Vec<RC<RCBase>> &vec, uint64_t idx) -> void * {
                if (idx >= vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return from_vector(vec[idx]);
            })
            .def("__setitem__", [&](Vec<RC<RCBase>> &vec, uint64_t idx, void *ptr) {
                if (idx >= vec.size()) [[unlikely]] {
                    LUISA_ERROR("index out of range");
                }
                return vec[idx] = (RCBase *)(ptr);
            });
        auto create_vector = [&]<typename T>(char const *name) {
            py::class_<Vec<T>>(m, name)
                .def(py::init<>())
                .def("clear", [&](Vec<T> &vec) {
                    vec.clear();
                })
                .def("__len__", [&](Vec<T> &vec) {
                    return vec.size();
                })
                .def_property_readonly("capacity", [&](Vec<T> &vec) {
                    return vec.capacity();
                })
                .def_property_readonly("empty", [&](Vec<T> &vec) {
                    return vec.empty();
                })
                .def("reserve", [&](Vec<T> &vec, uint64_t capacity) {
                    vec.reserve(capacity);
                })
                .def("emplace_back", [&](Vec<T> &vec, T ptr) {
                    vec.emplace_back(ptr);
                })
                .def("pop_back", [&](Vec<T> &vec) {
                    if (vec.empty()) [[unlikely]] {
                        LUISA_ERROR("call pop_back() in empty vector.");
                    }
                    vec.pop_back();
                })
                .def("back", [&](Vec<T> &vec) -> T {
                    if (vec.empty()) [[unlikely]] {
                        LUISA_ERROR("call back() in empty vector.");
                    }
                    return vec.back();
                })
                .def("__getitem__", [&](Vec<T> &vec, uint64_t idx) -> T {
                    if (idx >= vec.size()) [[unlikely]] {
                        LUISA_ERROR("index out of range");
                    }
                    return vec[idx];
                })
                .def("__setitem__", [&](Vec<T> &vec, uint64_t idx, T ptr) {
                    if (idx >= vec.size()) [[unlikely]] {
                        LUISA_ERROR("index out of range");
                    }
                    return vec[idx] = ptr;
                });
        };
        auto create_vector_cast = [&]<typename T, typename ElemT, typename ToElem, typename FromElem>(char const *name, ToElem &&to_elem, FromElem &&from_elem) {
            py::class_<Vec<T>>(m, name)
                .def(py::init<>())
                .def("clear", [&](Vec<T> &vec) {
                    vec.clear();
                })
                .def("__len__", [&](Vec<T> &vec) {
                    return vec.size();
                })
                .def_property_readonly("capacity", [&](Vec<T> &vec) {
                    return vec.capacity();
                })
                .def_property_readonly("empty", [&](Vec<T> &vec) {
                    return vec.empty();
                })
                .def("reserve", [&](Vec<T> &vec, uint64_t capacity) {
                    vec.reserve(capacity);
                })
                .def("emplace_back", [&](Vec<T> &vec, ElemT ptr) {
                    vec.emplace_back(from_elem(ptr));
                })
                .def("pop_back", [&](Vec<T> &vec) {
                    if (vec.empty()) [[unlikely]] {
                        LUISA_ERROR("call pop_back() in empty vector.");
                    }
                    vec.pop_back();
                })
                .def("back", [&](Vec<T> &vec) -> ElemT {
                    if (vec.empty()) [[unlikely]] {
                        LUISA_ERROR("call back() in empty vector.");
                    }
                    return to_elem(vec.back());
                })
                .def("__getitem__", [&](Vec<T> &vec, uint64_t idx) -> ElemT {
                    if (idx >= vec.size()) [[unlikely]] {
                        LUISA_ERROR("index out of range");
                    }
                    return to_elem(vec[idx]);
                })
                .def("__setitem__", [&](Vec<T> &vec, uint64_t idx, ElemT ptr) {
                    if (idx >= vec.size()) [[unlikely]] {
                        LUISA_ERROR("index out of range");
                    }
                    return vec[idx] = from_elem(ptr);
                });
        };

        create_vector.operator()<int>("int_vector");
        create_vector.operator()<uint>("uint_vector");
        create_vector.operator()<float4x4>("float4x4_vector");
        create_vector_cast.operator()<luisa::string, const char *>(
            "string_vector",
            [](luisa::string const &str) {
                return str.c_str();
            },
            [](const char *str) {
                return luisa::string(str);
            });
    }
}
static ModuleRegister module_register_export_vectors(export_vectors);