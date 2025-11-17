#include <nanobind/nanobind.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/rhi/command.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/hit.h>
#include <luisa/runtime/rtx/ray.h>
#include "module_register.h"
namespace nb = nanobind;
using namespace luisa;
using namespace luisa::compute;
// constexpr auto pyref = nb::rv_policy::reference; // object lifetime is managed on C++ side

void export_matrix(nb::module_& m)
{
    nb::class_<float2x2>(m, "float2x2")
        // .def("identity", [](){return float2x2();})
        .def("__getitem__", [](float2x2& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](float2x2& self, size_t i, float2 k) { self[i] = k; })
        .def("copy", [](float2x2& self) { return float2x2(self); })
        .def("__init__", [](float a) { return make_float2x2(a); })
        .def("__init__", [](float a, float b, float c, float d) { return make_float2x2(a, b, c, d); })
        .def("__init__", [](float2 a, float2 b) { return make_float2x2(a, b); })
        .def("__init__", [](float2x2 a) { return make_float2x2(a); })
        .def("__init__", [](float3x3 a) { return make_float2x2(a); })
        .def("__init__", [](float4x4 a) { return make_float2x2(a); })
        .def("__repr__", [](float2x2& self) { return to_nb_str(format("float2x2([{},{}], [{},{}])", self[0].x, self[0].y, self[1].x, self[1].y)); });

    nb::class_<float3x3>(m, "float3x3")
        // .def("identity", [](){return float3x3();})
        .def("__getitem__", [](float3x3& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](float3x3& self, size_t i, float3 k) { self[i] = k; })
        .def("copy", [](float3x3& self) { return float3x3(self); })
        .def("__init__", [](float a) { return make_float3x3(a); })
        .def("__init__", [](float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22) { return make_float3x3(m00, m01, m02, m10, m11, m12, m20, m21, m22); })
        .def("__init__", [](float3 a, float3 b, float3 c) { return make_float3x3(a, b, c); })
        .def("__init__", [](float2x2 a) { return make_float3x3(a); })
        .def("__init__", [](float3x3 a) { return make_float3x3(a); })
        .def("__init__", [](float4x4 a) { return make_float3x3(a); })
        .def("__repr__", [](float3x3& self) { return to_nb_str(format("float3x3([{},{},{}], [{},{},{}], [{},{},{}])", self[0].x, self[0].y, self[0].z, self[1].x, self[1].y, self[1].z, self[2].x, self[2].y, self[2].z)); });

    nb::class_<float4x4>(m, "float4x4")
        // .def("identity", [](){return float4x4();})
        .def("__getitem__", [](float4x4& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](float4x4& self, size_t i, float4 k) { self[i] = k; })
        .def("copy", [](float4x4& self) { return float4x4(self); })
        .def("__init__", [](float a) { return make_float4x4(a); })
        .def("__init__", [](float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33) { return make_float4x4(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33); })
        .def("__init__", [](float4 a, float4 b, float4 c, float4 d) { return make_float4x4(a, b, c, d); })
        .def("__init__", [](float2x2 a) { return make_float4x4(a); })
        .def("__init__", [](float3x3 a) { return make_float4x4(a); })
        .def("__init__", [](float4x4 a) { return make_float4x4(a); })
        .def("__repr__", [](float4x4& self) { return to_nb_str(format("float4x4([{},{},{},{}], [{},{},{},{}], [{},{},{},{}], [{},{},{},{}])", self[0].x, self[0].y, self[0].z, self[0].w, self[1].x, self[1].y, self[1].z, self[1].w, self[2].x, self[2].y, self[2].z, self[2].w, self[3].x, self[3].y, self[3].z, self[3].w)); });

    m.def("make_float2x2", [](float a) { return make_float2x2(a); });
    m.def("make_float2x2", [](float a, float b, float c, float d) { return make_float2x2(a, b, c, d); });
    m.def("make_float2x2", [](float2 a, float2 b) { return make_float2x2(a, b); });
    m.def("make_float2x2", [](float2x2 a) { return make_float2x2(a); });
    m.def("make_float2x2", [](float3x3 a) { return make_float2x2(a); });
    m.def("make_float2x2", [](float4x4 a) { return make_float2x2(a); });

    m.def("make_float3x3", [](float a) { return make_float3x3(a); });
    m.def("make_float3x3", [](float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22) { return make_float3x3(m00, m01, m02, m10, m11, m12, m20, m21, m22); });
    m.def("make_float3x3", [](float3 a, float3 b, float3 c) { return make_float3x3(a, b, c); });
    m.def("make_float3x3", [](float2x2 a) { return make_float3x3(a); });
    m.def("make_float3x3", [](float3x3 a) { return make_float3x3(a); });
    m.def("make_float3x3", [](float4x4 a) { return make_float3x3(a); });

    m.def("make_float4x4", [](float a) { return make_float4x4(a); });
    m.def("make_float4x4", [](float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33) { return make_float4x4(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33); });
    m.def("make_float4x4", [](float4 a, float4 b, float4 c, float4 d) { return make_float4x4(a, b, c, d); });
    m.def("make_float4x4", [](float2x2 a) { return make_float4x4(a); });
    m.def("make_float4x4", [](float3x3 a) { return make_float4x4(a); });
    m.def("make_float4x4", [](float4x4 a) { return make_float4x4(a); });
    m.def("inverse", [](float2x2 a) { return inverse(a); });
    m.def("inverse", [](float3x3 a) { return inverse(a); });
    m.def("inverse", [](float4x4 a) { return inverse(a); });
    m.def("transpose", [](float2x2 a) { return transpose(a); });
    m.def("transpose", [](float3x3 a) { return transpose(a); });
    m.def("transpose", [](float4x4 a) { return transpose(a); });
    m.def("determinant", [](float2x2 a) { return determinant(a); });
    m.def("determinant", [](float3x3 a) { return determinant(a); });
    m.def("determinant", [](float4x4 a) { return determinant(a); });
    // TODO export matrix operators
    nb::class_<double2x2>(m, "double2x2")
        // .def("identity", [](){return double2x2();})
        .def("__getitem__", [](double2x2& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](double2x2& self, size_t i, double2 k) { self[i] = k; })
        .def("copy", [](double2x2& self) { return double2x2(self); })
        .def("__init__", [](double a) { return make_double2x2(a); })
        .def("__init__", [](double a, double b, double c, double d) { return make_double2x2(a, b, c, d); })
        .def("__init__", [](double2 a, double2 b) { return make_double2x2(a, b); })
        .def("__init__", [](double2x2 a) { return make_double2x2(a); })
        .def("__init__", [](double3x3 a) { return make_double2x2(a); })
        .def("__init__", [](double4x4 a) { return make_double2x2(a); })
        .def("__repr__", [](double2x2& self) { return to_nb_str(format("double2x2([{},{}], [{},{}])", self[0].x, self[0].y, self[1].x, self[1].y)); });

    nb::class_<double3x3>(m, "double3x3")
        // .def("identity", [](){return double3x3();})
        .def("__getitem__", [](double3x3& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](double3x3& self, size_t i, double3 k) { self[i] = k; })
        .def("copy", [](double3x3& self) { return double3x3(self); })
        .def("__init__", [](double a) { return make_double3x3(a); })
        .def("__init__", [](double m00, double m01, double m02, double m10, double m11, double m12, double m20, double m21, double m22) { return make_double3x3(m00, m01, m02, m10, m11, m12, m20, m21, m22); })
        .def("__init__", [](double3 a, double3 b, double3 c) { return make_double3x3(a, b, c); })
        .def("__init__", [](double2x2 a) { return make_double3x3(a); })
        .def("__init__", [](double3x3 a) { return make_double3x3(a); })
        .def("__init__", [](double4x4 a) { return make_double3x3(a); })
        .def("__repr__", [](double3x3& self) { return to_nb_str(format("double3x3([{},{},{}], [{},{},{}], [{},{},{}])", self[0].x, self[0].y, self[0].z, self[1].x, self[1].y, self[1].z, self[2].x, self[2].y, self[2].z)); });

    nb::class_<double4x4>(m, "double4x4")
        // .def("identity", [](){return double4x4();})
        .def("__getitem__", [](double4x4& self, size_t i) { return &self[i]; }, nb::rv_policy::reference_internal)
        .def("__setitem__", [](double4x4& self, size_t i, double4 k) { self[i] = k; })
        .def("copy", [](double4x4& self) { return double4x4(self); })
        .def("__init__", [](double a) { return make_double4x4(a); })
        .def("__init__", [](double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23, double m30, double m31, double m32, double m33) { return make_double4x4(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33); })
        .def("__init__", [](double4 a, double4 b, double4 c, double4 d) { return make_double4x4(a, b, c, d); })
        .def("__init__", [](double2x2 a) { return make_double4x4(a); })
        .def("__init__", [](double3x3 a) { return make_double4x4(a); })
        .def("__init__", [](double4x4 a) { return make_double4x4(a); })
        .def("__repr__", [](double4x4& self) { return to_nb_str(format("double4x4([{},{},{},{}], [{},{},{},{}], [{},{},{},{}], [{},{},{},{}])", self[0].x, self[0].y, self[0].z, self[0].w, self[1].x, self[1].y, self[1].z, self[1].w, self[2].x, self[2].y, self[2].z, self[2].w, self[3].x, self[3].y, self[3].z, self[3].w)); });

    m.def("make_double2x2", [](double a) { return make_double2x2(a); });
    m.def("make_double2x2", [](double a, double b, double c, double d) { return make_double2x2(a, b, c, d); });
    m.def("make_double2x2", [](double2 a, double2 b) { return make_double2x2(a, b); });
    m.def("make_double2x2", [](double2x2 a) { return make_double2x2(a); });
    m.def("make_double2x2", [](double3x3 a) { return make_double2x2(a); });
    m.def("make_double2x2", [](double4x4 a) { return make_double2x2(a); });

    m.def("make_double3x3", [](double a) { return make_double3x3(a); });
    m.def("make_double3x3", [](double m00, double m01, double m02, double m10, double m11, double m12, double m20, double m21, double m22) { return make_double3x3(m00, m01, m02, m10, m11, m12, m20, m21, m22); });
    m.def("make_double3x3", [](double3 a, double3 b, double3 c) { return make_double3x3(a, b, c); });
    m.def("make_double3x3", [](double2x2 a) { return make_double3x3(a); });
    m.def("make_double3x3", [](double3x3 a) { return make_double3x3(a); });
    m.def("make_double3x3", [](double4x4 a) { return make_double3x3(a); });

    m.def("make_double4x4", [](double a) { return make_double4x4(a); });
    m.def("make_double4x4", [](double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23, double m30, double m31, double m32, double m33) { return make_double4x4(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33); });
    m.def("make_double4x4", [](double4 a, double4 b, double4 c, double4 d) { return make_double4x4(a, b, c, d); });
    m.def("make_double4x4", [](double2x2 a) { return make_double4x4(a); });
    m.def("make_double4x4", [](double3x3 a) { return make_double4x4(a); });
    m.def("make_double4x4", [](double4x4 a) { return make_double4x4(a); });
    m.def("inverse", [](double2x2 a) { return inverse(a); });
    m.def("inverse", [](double3x3 a) { return inverse(a); });
    m.def("inverse", [](double4x4 a) { return inverse(a); });
    m.def("transpose", [](double2x2 a) { return transpose(a); });
    m.def("transpose", [](double3x3 a) { return transpose(a); });
    m.def("transpose", [](double4x4 a) { return transpose(a); });
    m.def("determinant", [](double2x2 a) { return determinant(a); });
    m.def("determinant", [](double3x3 a) { return determinant(a); });
    m.def("determinant", [](double4x4 a) { return determinant(a); });
}

static ModuleRegister _module_register(export_matrix);