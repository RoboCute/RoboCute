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

#define LUISA_EXPORT_ARITHMETIC_OP(T)                                                                              \
    m##T                                                                                                           \
        .def("__add__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a + b; }, nb::is_operator())     \
        .def("__add__", [](const Vector<T, 3> &a, const T &b) { return a + b; }, nb::is_operator())                \
        .def("__radd__", [](const Vector<T, 3> &a, const T &b) { return a + b; }, nb::is_operator())               \
        .def("__sub__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a - b; }, nb::is_operator())     \
        .def("__sub__", [](const Vector<T, 3> &a, const T &b) { return a - b; }, nb::is_operator())                \
        .def("__rsub__", [](const Vector<T, 3> &a, const T &b) { return b - a; }, nb::is_operator())               \
        .def("__mul__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a * b; }, nb::is_operator())     \
        .def("__mul__", [](const Vector<T, 3> &a, const T &b) { return a * b; }, nb::is_operator())                \
        .def("__rmul__", [](const Vector<T, 3> &a, const T &b) { return a * b; }, nb::is_operator())               \
        .def("__truediv__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a / b; }, nb::is_operator()) \
        .def("__truediv__", [](const Vector<T, 3> &a, const T &b) { return a / b; }, nb::is_operator())            \
        .def("__rtruediv__", [](const Vector<T, 3> &a, const T &b) { return b / a; }, nb::is_operator())           \
        .def("__gt__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a > b; }, nb::is_operator())      \
        .def("__ge__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a >= b; }, nb::is_operator())     \
        .def("__lt__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a < b; }, nb::is_operator())      \
        .def("__le__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a <= b; }, nb::is_operator())     \
        .def("__eq__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a == b; }, nb::is_operator())     \
        .def("__ne__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a != b; }, nb::is_operator());

#define LUISA_EXPORT_BOOL_OP(T)                                                                                 \
    m##T                                                                                                        \
        .def("__eq__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a == b; }, nb::is_operator())  \
        .def("__ne__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a != b; }, nb::is_operator())  \
        .def("__and__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a && b; }, nb::is_operator()) \
        .def("__or__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a || b; }, nb::is_operator());

#define LUISA_EXPORT_INT_OP(T)                                                                                 \
    m##T                                                                                                       \
        .def("__mod__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a % b; }, nb::is_operator()) \
        .def("__mod__", [](const Vector<T, 3> &a, const T &b) { return a % b; }, nb::is_operator())            \
        .def("__rmod__", [](const Vector<T, 3> &a, const T &b) { return b % a; }, nb::is_operator())           \
        .def("__shl__", [](const Vector<T, 3> &a, const T &b) { return a << b; }, nb::is_operator())           \
        .def("__shr__", [](const Vector<T, 3> &a, const T &b) { return a >> b; }, nb::is_operator())           \
        .def("__xor__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return a ^ b; }, nb::is_operator());

#define LUISA_EXPORT_FLOAT_OP(T) \
    m##T                         \
        .def("__pow__", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::pow(a, b); }, nb::is_operator());

#define LUISA_EXPORT_UNARY_FUNC(T, name) \
    m.def(#name, [](const Vector<T, 3> &v) { return luisa::name(v); });

#define LUISA_EXPORT_ARITHMETIC_FUNC(T)                                                                                 \
    m.def("min", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::min(a, b); });                        \
    m.def("min", [](const Vector<T, 3> &a, const T &b) { return luisa::min(a, b); });                                   \
    m.def("min", [](const T &a, const Vector<T, 3> &b) { return luisa::min(a, b); });                                   \
    m.def("max", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::max(a, b); });                        \
    m.def("max", [](const Vector<T, 3> &a, const T &b) { return luisa::max(a, b); });                                   \
    m.def("max", [](const T &a, const Vector<T, 3> &b) { return luisa::max(a, b); });                                   \
    m.def("select", [](const Vector<T, 3> &a, const Vector<T, 3> &b, bool pred) { return luisa::select(a, b, pred); }); \
    m.def("clamp", [](const Vector<T, 3> &v, const T &a, const T &b) { return luisa::clamp(v, a, b); });

#define LUISA_EXPORT_FLOAT_FUNC(T)                                                                             \
    m.def("pow", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::pow(a, b); });               \
    m.def("atan2", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::atan2(a, b); });           \
    m.def("lerp", [](const Vector<T, 3> &a, const Vector<T, 3> &b, T t) { return luisa::lerp(a, b, t); }); \
    m.def("dot", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::dot(a, b); });               \
    m.def("distance", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::distance(a, b); });     \
    m.def("cross", [](const Vector<T, 3> &a, const Vector<T, 3> &b) { return luisa::cross(a, b); });           \
    m.def("rotation", [](const Vector<T, 3> &a, const T &b) { return luisa::rotation(a, b); });                \
    m.def("scaling", [](const Vector<T, 3> &v) { return luisa::scaling(v); });                                 \
    m.def("scaling", [](const T &v) { return luisa::scaling(v); });                                            \
    LUISA_EXPORT_UNARY_FUNC(T, acos)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, asin)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, atan)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, cos)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, sin)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, tan)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, sqrt)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, ceil)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, floor)                                                                          \
    LUISA_EXPORT_UNARY_FUNC(T, round)                                                                          \
    LUISA_EXPORT_UNARY_FUNC(T, exp)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, log)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, log10)                                                                          \
    LUISA_EXPORT_UNARY_FUNC(T, log2)                                                                           \
    LUISA_EXPORT_UNARY_FUNC(T, abs)                                                                            \
    LUISA_EXPORT_UNARY_FUNC(T, radians)                                                                        \
    LUISA_EXPORT_UNARY_FUNC(T, degrees)                                                                        \
    LUISA_EXPORT_UNARY_FUNC(T, length)                                                                         \
    LUISA_EXPORT_UNARY_FUNC(T, normalize)                                                                      \
    LUISA_EXPORT_UNARY_FUNC(T, translation)

#define LUISA_EXPORT_VECTOR3(T)                                                                                           \
    nb::class_<luisa::detail::VectorStorage<T, 3>>(m, "_vectorstorage_" #T "3");                                          \
    auto m##T = nb::class_<Vector<T, 3>, luisa::detail::VectorStorage<T, 3>>(m, #T "3")                                   \
                    .def(nb::init<T>())                                                                                   \
                    .def(nb::init<T, T, T>())                                                                             \
                    .def(nb::init<Vector<T, 3>>())                                                                        \
                    .def("__repr__", [](Vector<T, 3> &self) { return to_nb_str(format(#T "3({},{},{})", self.x, self.y, self.z)); }) \
                    .def("__getitem__", [](Vector<T, 3> &self, size_t i) { return self[i]; })                             \
                    .def("__setitem__", [](Vector<T, 3> &self, size_t i, T k) { self[i] = k; })                           \
                    .def("copy", [](Vector<T, 3> &self) { return Vector<T, 3>(self); })                                   \
                    .def_rw("x", &Vector<T, 3>::x)                                                                 \
                    .def_rw("y", &Vector<T, 3>::y)                                                                 \
                    .def_rw("z", &Vector<T, 3>::z)                                                                 \
                    .def_prop_ro("xx", &Vector<T, 3>::xx)                                                       \
                    .def_prop_ro("xy", &Vector<T, 3>::xy)                                                       \
                    .def_prop_ro("xz", &Vector<T, 3>::xz)                                                       \
                    .def_prop_ro("yx", &Vector<T, 3>::yx)                                                       \
                    .def_prop_ro("yy", &Vector<T, 3>::yy)                                                       \
                    .def_prop_ro("yz", &Vector<T, 3>::yz)                                                       \
                    .def_prop_ro("zx", &Vector<T, 3>::zx)                                                       \
                    .def_prop_ro("zy", &Vector<T, 3>::zy)                                                       \
                    .def_prop_ro("zz", &Vector<T, 3>::zz)                                                       \
                    .def_prop_ro("xxx", &Vector<T, 3>::xxx)                                                     \
                    .def_prop_ro("xxy", &Vector<T, 3>::xxy)                                                     \
                    .def_prop_ro("xxz", &Vector<T, 3>::xxz)                                                     \
                    .def_prop_ro("xyx", &Vector<T, 3>::xyx)                                                     \
                    .def_prop_ro("xyy", &Vector<T, 3>::xyy)                                                     \
                    .def_prop_ro("xyz", &Vector<T, 3>::xyz)                                                     \
                    .def_prop_ro("xzx", &Vector<T, 3>::xzx)                                                     \
                    .def_prop_ro("xzy", &Vector<T, 3>::xzy)                                                     \
                    .def_prop_ro("xzz", &Vector<T, 3>::xzz)                                                     \
                    .def_prop_ro("yxx", &Vector<T, 3>::yxx)                                                     \
                    .def_prop_ro("yxy", &Vector<T, 3>::yxy)                                                     \
                    .def_prop_ro("yxz", &Vector<T, 3>::yxz)                                                     \
                    .def_prop_ro("yyx", &Vector<T, 3>::yyx)                                                     \
                    .def_prop_ro("yyy", &Vector<T, 3>::yyy)                                                     \
                    .def_prop_ro("yyz", &Vector<T, 3>::yyz)                                                     \
                    .def_prop_ro("yzx", &Vector<T, 3>::yzx)                                                     \
                    .def_prop_ro("yzy", &Vector<T, 3>::yzy)                                                     \
                    .def_prop_ro("yzz", &Vector<T, 3>::yzz)                                                     \
                    .def_prop_ro("zxx", &Vector<T, 3>::zxx)                                                     \
                    .def_prop_ro("zxy", &Vector<T, 3>::zxy)                                                     \
                    .def_prop_ro("zxz", &Vector<T, 3>::zxz)                                                     \
                    .def_prop_ro("zyx", &Vector<T, 3>::zyx)                                                     \
                    .def_prop_ro("zyy", &Vector<T, 3>::zyy)                                                     \
                    .def_prop_ro("zyz", &Vector<T, 3>::zyz)                                                     \
                    .def_prop_ro("zzx", &Vector<T, 3>::zzx)                                                     \
                    .def_prop_ro("zzy", &Vector<T, 3>::zzy)                                                     \
                    .def_prop_ro("zzz", &Vector<T, 3>::zzz)                                                     \
                    .def_prop_ro("xxxx", &Vector<T, 3>::xxxx)                                                   \
                    .def_prop_ro("xxxy", &Vector<T, 3>::xxxy)                                                   \
                    .def_prop_ro("xxxz", &Vector<T, 3>::xxxz)                                                   \
                    .def_prop_ro("xxyx", &Vector<T, 3>::xxyx)                                                   \
                    .def_prop_ro("xxyy", &Vector<T, 3>::xxyy)                                                   \
                    .def_prop_ro("xxyz", &Vector<T, 3>::xxyz)                                                   \
                    .def_prop_ro("xxzx", &Vector<T, 3>::xxzx)                                                   \
                    .def_prop_ro("xxzy", &Vector<T, 3>::xxzy)                                                   \
                    .def_prop_ro("xxzz", &Vector<T, 3>::xxzz)                                                   \
                    .def_prop_ro("xyxx", &Vector<T, 3>::xyxx)                                                   \
                    .def_prop_ro("xyxy", &Vector<T, 3>::xyxy)                                                   \
                    .def_prop_ro("xyxz", &Vector<T, 3>::xyxz)                                                   \
                    .def_prop_ro("xyyx", &Vector<T, 3>::xyyx)                                                   \
                    .def_prop_ro("xyyy", &Vector<T, 3>::xyyy)                                                   \
                    .def_prop_ro("xyyz", &Vector<T, 3>::xyyz)                                                   \
                    .def_prop_ro("xyzx", &Vector<T, 3>::xyzx)                                                   \
                    .def_prop_ro("xyzy", &Vector<T, 3>::xyzy)                                                   \
                    .def_prop_ro("xyzz", &Vector<T, 3>::xyzz)                                                   \
                    .def_prop_ro("xzxx", &Vector<T, 3>::xzxx)                                                   \
                    .def_prop_ro("xzxy", &Vector<T, 3>::xzxy)                                                   \
                    .def_prop_ro("xzxz", &Vector<T, 3>::xzxz)                                                   \
                    .def_prop_ro("xzyx", &Vector<T, 3>::xzyx)                                                   \
                    .def_prop_ro("xzyy", &Vector<T, 3>::xzyy)                                                   \
                    .def_prop_ro("xzyz", &Vector<T, 3>::xzyz)                                                   \
                    .def_prop_ro("xzzx", &Vector<T, 3>::xzzx)                                                   \
                    .def_prop_ro("xzzy", &Vector<T, 3>::xzzy)                                                   \
                    .def_prop_ro("xzzz", &Vector<T, 3>::xzzz)                                                   \
                    .def_prop_ro("yxxx", &Vector<T, 3>::yxxx)                                                   \
                    .def_prop_ro("yxxy", &Vector<T, 3>::yxxy)                                                   \
                    .def_prop_ro("yxxz", &Vector<T, 3>::yxxz)                                                   \
                    .def_prop_ro("yxyx", &Vector<T, 3>::yxyx)                                                   \
                    .def_prop_ro("yxyy", &Vector<T, 3>::yxyy)                                                   \
                    .def_prop_ro("yxyz", &Vector<T, 3>::yxyz)                                                   \
                    .def_prop_ro("yxzx", &Vector<T, 3>::yxzx)                                                   \
                    .def_prop_ro("yxzy", &Vector<T, 3>::yxzy)                                                   \
                    .def_prop_ro("yxzz", &Vector<T, 3>::yxzz)                                                   \
                    .def_prop_ro("yyxx", &Vector<T, 3>::yyxx)                                                   \
                    .def_prop_ro("yyxy", &Vector<T, 3>::yyxy)                                                   \
                    .def_prop_ro("yyxz", &Vector<T, 3>::yyxz)                                                   \
                    .def_prop_ro("yyyx", &Vector<T, 3>::yyyx)                                                   \
                    .def_prop_ro("yyyy", &Vector<T, 3>::yyyy)                                                   \
                    .def_prop_ro("yyyz", &Vector<T, 3>::yyyz)                                                   \
                    .def_prop_ro("yyzx", &Vector<T, 3>::yyzx)                                                   \
                    .def_prop_ro("yyzy", &Vector<T, 3>::yyzy)                                                   \
                    .def_prop_ro("yyzz", &Vector<T, 3>::yyzz)                                                   \
                    .def_prop_ro("yzxx", &Vector<T, 3>::yzxx)                                                   \
                    .def_prop_ro("yzxy", &Vector<T, 3>::yzxy)                                                   \
                    .def_prop_ro("yzxz", &Vector<T, 3>::yzxz)                                                   \
                    .def_prop_ro("yzyx", &Vector<T, 3>::yzyx)                                                   \
                    .def_prop_ro("yzyy", &Vector<T, 3>::yzyy)                                                   \
                    .def_prop_ro("yzyz", &Vector<T, 3>::yzyz)                                                   \
                    .def_prop_ro("yzzx", &Vector<T, 3>::yzzx)                                                   \
                    .def_prop_ro("yzzy", &Vector<T, 3>::yzzy)                                                   \
                    .def_prop_ro("yzzz", &Vector<T, 3>::yzzz)                                                   \
                    .def_prop_ro("zxxx", &Vector<T, 3>::zxxx)                                                   \
                    .def_prop_ro("zxxy", &Vector<T, 3>::zxxy)                                                   \
                    .def_prop_ro("zxxz", &Vector<T, 3>::zxxz)                                                   \
                    .def_prop_ro("zxyx", &Vector<T, 3>::zxyx)                                                   \
                    .def_prop_ro("zxyy", &Vector<T, 3>::zxyy)                                                   \
                    .def_prop_ro("zxyz", &Vector<T, 3>::zxyz)                                                   \
                    .def_prop_ro("zxzx", &Vector<T, 3>::zxzx)                                                   \
                    .def_prop_ro("zxzy", &Vector<T, 3>::zxzy)                                                   \
                    .def_prop_ro("zxzz", &Vector<T, 3>::zxzz)                                                   \
                    .def_prop_ro("zyxx", &Vector<T, 3>::zyxx)                                                   \
                    .def_prop_ro("zyxy", &Vector<T, 3>::zyxy)                                                   \
                    .def_prop_ro("zyxz", &Vector<T, 3>::zyxz)                                                   \
                    .def_prop_ro("zyyx", &Vector<T, 3>::zyyx)                                                   \
                    .def_prop_ro("zyyy", &Vector<T, 3>::zyyy)                                                   \
                    .def_prop_ro("zyyz", &Vector<T, 3>::zyyz)                                                   \
                    .def_prop_ro("zyzx", &Vector<T, 3>::zyzx)                                                   \
                    .def_prop_ro("zyzy", &Vector<T, 3>::zyzy)                                                   \
                    .def_prop_ro("zyzz", &Vector<T, 3>::zyzz)                                                   \
                    .def_prop_ro("zzxx", &Vector<T, 3>::zzxx)                                                   \
                    .def_prop_ro("zzxy", &Vector<T, 3>::zzxy)                                                   \
                    .def_prop_ro("zzxz", &Vector<T, 3>::zzxz)                                                   \
                    .def_prop_ro("zzyx", &Vector<T, 3>::zzyx)                                                   \
                    .def_prop_ro("zzyy", &Vector<T, 3>::zzyy)                                                   \
                    .def_prop_ro("zzyz", &Vector<T, 3>::zzyz)                                                   \
                    .def_prop_ro("zzzx", &Vector<T, 3>::zzzx)                                                   \
                    .def_prop_ro("zzzy", &Vector<T, 3>::zzzy)                                                   \
                    .def_prop_ro("zzzz", &Vector<T, 3>::zzzz);                                                  \
    m.def("make_" #T "3", [](T a) { return make_##T##3(a); });                                                            \
    m.def("make_" #T "3", [](T a, T b, T c) { return make_##T##3(a, b, c); });                                            \
    m.def("make_" #T "3", [](Vector<T, 3> a) { return make_##T##3(a); });

void export_vector3(nb::module_ &m) {
    LUISA_EXPORT_VECTOR3(bool)
    LUISA_EXPORT_VECTOR3(uint)
    LUISA_EXPORT_VECTOR3(int)
    LUISA_EXPORT_VECTOR3(float)
    LUISA_EXPORT_VECTOR3(double)
    LUISA_EXPORT_ARITHMETIC_FUNC(int)
    LUISA_EXPORT_ARITHMETIC_FUNC(uint)
    LUISA_EXPORT_ARITHMETIC_FUNC(float)
    LUISA_EXPORT_ARITHMETIC_FUNC(double)
    LUISA_EXPORT_FLOAT_FUNC(float)
    LUISA_EXPORT_FLOAT_FUNC(double)
    LUISA_EXPORT_ARITHMETIC_OP(uint)
    LUISA_EXPORT_ARITHMETIC_OP(int)
    LUISA_EXPORT_ARITHMETIC_OP(float)
    LUISA_EXPORT_ARITHMETIC_OP(double)
    LUISA_EXPORT_INT_OP(uint)
    LUISA_EXPORT_INT_OP(int)
    LUISA_EXPORT_FLOAT_OP(float)
    LUISA_EXPORT_FLOAT_OP(double)
    LUISA_EXPORT_BOOL_OP(bool)
}
static ModuleRegister _module_register(export_vector3);