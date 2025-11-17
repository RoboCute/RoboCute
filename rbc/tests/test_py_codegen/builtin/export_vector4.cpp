#include <nanobind/nanobind.h>
#include <luisa/ast/function.h>
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
        .def("__add__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a + b; }, nb::is_operator())     \
        .def("__add__", [](const Vector<T, 4>& a, const T& b) { return a + b; }, nb::is_operator())                \
        .def("__radd__", [](const Vector<T, 4>& a, const T& b) { return a + b; }, nb::is_operator())               \
        .def("__sub__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a - b; }, nb::is_operator())     \
        .def("__sub__", [](const Vector<T, 4>& a, const T& b) { return a - b; }, nb::is_operator())                \
        .def("__rsub__", [](const Vector<T, 4>& a, const T& b) { return b - a; }, nb::is_operator())               \
        .def("__mul__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a * b; }, nb::is_operator())     \
        .def("__mul__", [](const Vector<T, 4>& a, const T& b) { return a * b; }, nb::is_operator())                \
        .def("__rmul__", [](const Vector<T, 4>& a, const T& b) { return a * b; }, nb::is_operator())               \
        .def("__truediv__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a / b; }, nb::is_operator()) \
        .def("__truediv__", [](const Vector<T, 4>& a, const T& b) { return a / b; }, nb::is_operator())            \
        .def("__rtruediv__", [](const Vector<T, 4>& a, const T& b) { return b / a; }, nb::is_operator())           \
        .def("__gt__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a > b; }, nb::is_operator())      \
        .def("__ge__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a >= b; }, nb::is_operator())     \
        .def("__lt__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a < b; }, nb::is_operator())      \
        .def("__le__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a <= b; }, nb::is_operator())     \
        .def("__eq__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a == b; }, nb::is_operator())     \
        .def("__ne__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a != b; }, nb::is_operator());

#define LUISA_EXPORT_BOOL_OP(T)                                                                                 \
    m##T                                                                                                        \
        .def("__eq__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a == b; }, nb::is_operator())  \
        .def("__ne__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a != b; }, nb::is_operator())  \
        .def("__and__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a && b; }, nb::is_operator()) \
        .def("__or__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a || b; }, nb::is_operator());

#define LUISA_EXPORT_INT_OP(T)                                                                                 \
    m##T                                                                                                       \
        .def("__mod__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a % b; }, nb::is_operator()) \
        .def("__mod__", [](const Vector<T, 4>& a, const T& b) { return a % b; }, nb::is_operator())            \
        .def("__rmod__", [](const Vector<T, 4>& a, const T& b) { return b % a; }, nb::is_operator())           \
        .def("__shl__", [](const Vector<T, 4>& a, const T& b) { return a << b; }, nb::is_operator())           \
        .def("__shr__", [](const Vector<T, 4>& a, const T& b) { return a >> b; }, nb::is_operator())           \
        .def("__xor__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return a ^ b; }, nb::is_operator());

#define LUISA_EXPORT_FLOAT_OP(T) \
    m##T                         \
        .def("__pow__", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::pow(a, b); }, nb::is_operator());

#define LUISA_EXPORT_UNARY_FUNC(T, name) \
    m.def(#name, [](const Vector<T, 4>& v) { return luisa::name(v); });

#define LUISA_EXPORT_ARITHMETIC_FUNC(T)                                                                                 \
    m.def("min", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::min(a, b); });                        \
    m.def("min", [](const Vector<T, 4>& a, const T& b) { return luisa::min(a, b); });                                   \
    m.def("min", [](const T& a, const Vector<T, 4>& b) { return luisa::min(a, b); });                                   \
    m.def("max", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::max(a, b); });                        \
    m.def("max", [](const Vector<T, 4>& a, const T& b) { return luisa::max(a, b); });                                   \
    m.def("max", [](const T& a, const Vector<T, 4>& b) { return luisa::max(a, b); });                                   \
    m.def("select", [](const Vector<T, 4>& a, const Vector<T, 4>& b, bool pred) { return luisa::select(a, b, pred); }); \
    m.def("clamp", [](const Vector<T, 4>& v, const T& a, const T& b) { return luisa::clamp(v, a, b); });

#define LUISA_EXPORT_FLOAT_FUNC(T)                                                                             \
    m.def("pow", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::pow(a, b); });               \
    m.def("atan2", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::atan2(a, b); });           \
    m.def("lerp", [](const Vector<T, 4>& a, const Vector<T, 4>& b, T t) { return luisa::lerp(a, b, t); }); \
    m.def("dot", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::dot(a, b); });               \
    m.def("distance", [](const Vector<T, 4>& a, const Vector<T, 4>& b) { return luisa::distance(a, b); });     \
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
    LUISA_EXPORT_UNARY_FUNC(T, normalize)

#define LUISA_EXPORT_VECTOR4(T)                                                                                                      \
    nb::class_<luisa::detail::VectorStorage<T, 4>>(m, "_vectorstorage_" #T "4");                                                     \
    auto m##T = nb::class_<Vector<T, 4>, luisa::detail::VectorStorage<T, 4>>(m, #T "4")                                              \
                    .def(nb::init<T>())                                                                                              \
                    .def(nb::init<T, T, T, T>())                                                                                     \
                    .def(nb::init<Vector<T, 4>>())                                                                                   \
                    .def("__repr__", [](Vector<T, 4>& self) { return to_nb_str(format(#T "4({},{},{},{})", self.x, self.y, self.z, self.w)); }) \
                    .def("__getitem__", [](Vector<T, 4>& self, size_t i) { return self[i]; })                                        \
                    .def("__setitem__", [](Vector<T, 4>& self, size_t i, T k) { self[i] = k; })                                      \
                    .def("copy", [](Vector<T, 4>& self) { return Vector<T, 4>(self); })                                              \
                    .def_rw("x", &Vector<T, 4>::x)                                                                                   \
                    .def_rw("y", &Vector<T, 4>::y)                                                                                   \
                    .def_rw("z", &Vector<T, 4>::z)                                                                                   \
                    .def_rw("w", &Vector<T, 4>::w)                                                                                   \
                    .def_prop_ro("xx", &Vector<T, 4>::xx)                                                                            \
                    .def_prop_ro("xy", &Vector<T, 4>::xy)                                                                            \
                    .def_prop_ro("xz", &Vector<T, 4>::xz)                                                                            \
                    .def_prop_ro("xw", &Vector<T, 4>::xw)                                                                            \
                    .def_prop_ro("yx", &Vector<T, 4>::yx)                                                                            \
                    .def_prop_ro("yy", &Vector<T, 4>::yy)                                                                            \
                    .def_prop_ro("yz", &Vector<T, 4>::yz)                                                                            \
                    .def_prop_ro("yw", &Vector<T, 4>::yw)                                                                            \
                    .def_prop_ro("zx", &Vector<T, 4>::zx)                                                                            \
                    .def_prop_ro("zy", &Vector<T, 4>::zy)                                                                            \
                    .def_prop_ro("zz", &Vector<T, 4>::zz)                                                                            \
                    .def_prop_ro("zw", &Vector<T, 4>::zw)                                                                            \
                    .def_prop_ro("wx", &Vector<T, 4>::wx)                                                                            \
                    .def_prop_ro("wy", &Vector<T, 4>::wy)                                                                            \
                    .def_prop_ro("wz", &Vector<T, 4>::wz)                                                                            \
                    .def_prop_ro("ww", &Vector<T, 4>::ww)                                                                            \
                    .def_prop_ro("xxx", &Vector<T, 4>::xxx)                                                                          \
                    .def_prop_ro("xxy", &Vector<T, 4>::xxy)                                                                          \
                    .def_prop_ro("xxz", &Vector<T, 4>::xxz)                                                                          \
                    .def_prop_ro("xxw", &Vector<T, 4>::xxw)                                                                          \
                    .def_prop_ro("xyx", &Vector<T, 4>::xyx)                                                                          \
                    .def_prop_ro("xyy", &Vector<T, 4>::xyy)                                                                          \
                    .def_prop_ro("xyz", &Vector<T, 4>::xyz)                                                                          \
                    .def_prop_ro("xyw", &Vector<T, 4>::xyw)                                                                          \
                    .def_prop_ro("xzx", &Vector<T, 4>::xzx)                                                                          \
                    .def_prop_ro("xzy", &Vector<T, 4>::xzy)                                                                          \
                    .def_prop_ro("xzz", &Vector<T, 4>::xzz)                                                                          \
                    .def_prop_ro("xzw", &Vector<T, 4>::xzw)                                                                          \
                    .def_prop_ro("xwx", &Vector<T, 4>::xwx)                                                                          \
                    .def_prop_ro("xwy", &Vector<T, 4>::xwy)                                                                          \
                    .def_prop_ro("xwz", &Vector<T, 4>::xwz)                                                                          \
                    .def_prop_ro("xww", &Vector<T, 4>::xww)                                                                          \
                    .def_prop_ro("yxx", &Vector<T, 4>::yxx)                                                                          \
                    .def_prop_ro("yxy", &Vector<T, 4>::yxy)                                                                          \
                    .def_prop_ro("yxz", &Vector<T, 4>::yxz)                                                                          \
                    .def_prop_ro("yxw", &Vector<T, 4>::yxw)                                                                          \
                    .def_prop_ro("yyx", &Vector<T, 4>::yyx)                                                                          \
                    .def_prop_ro("yyy", &Vector<T, 4>::yyy)                                                                          \
                    .def_prop_ro("yyz", &Vector<T, 4>::yyz)                                                                          \
                    .def_prop_ro("yyw", &Vector<T, 4>::yyw)                                                                          \
                    .def_prop_ro("yzx", &Vector<T, 4>::yzx)                                                                          \
                    .def_prop_ro("yzy", &Vector<T, 4>::yzy)                                                                          \
                    .def_prop_ro("yzz", &Vector<T, 4>::yzz)                                                                          \
                    .def_prop_ro("yzw", &Vector<T, 4>::yzw)                                                                          \
                    .def_prop_ro("ywx", &Vector<T, 4>::ywx)                                                                          \
                    .def_prop_ro("ywy", &Vector<T, 4>::ywy)                                                                          \
                    .def_prop_ro("ywz", &Vector<T, 4>::ywz)                                                                          \
                    .def_prop_ro("yww", &Vector<T, 4>::yww)                                                                          \
                    .def_prop_ro("zxx", &Vector<T, 4>::zxx)                                                                          \
                    .def_prop_ro("zxy", &Vector<T, 4>::zxy)                                                                          \
                    .def_prop_ro("zxz", &Vector<T, 4>::zxz)                                                                          \
                    .def_prop_ro("zxw", &Vector<T, 4>::zxw)                                                                          \
                    .def_prop_ro("zyx", &Vector<T, 4>::zyx)                                                                          \
                    .def_prop_ro("zyy", &Vector<T, 4>::zyy)                                                                          \
                    .def_prop_ro("zyz", &Vector<T, 4>::zyz)                                                                          \
                    .def_prop_ro("zyw", &Vector<T, 4>::zyw)                                                                          \
                    .def_prop_ro("zzx", &Vector<T, 4>::zzx)                                                                          \
                    .def_prop_ro("zzy", &Vector<T, 4>::zzy)                                                                          \
                    .def_prop_ro("zzz", &Vector<T, 4>::zzz)                                                                          \
                    .def_prop_ro("zzw", &Vector<T, 4>::zzw)                                                                          \
                    .def_prop_ro("zwx", &Vector<T, 4>::zwx)                                                                          \
                    .def_prop_ro("zwy", &Vector<T, 4>::zwy)                                                                          \
                    .def_prop_ro("zwz", &Vector<T, 4>::zwz)                                                                          \
                    .def_prop_ro("zww", &Vector<T, 4>::zww)                                                                          \
                    .def_prop_ro("wxx", &Vector<T, 4>::wxx)                                                                          \
                    .def_prop_ro("wxy", &Vector<T, 4>::wxy)                                                                          \
                    .def_prop_ro("wxz", &Vector<T, 4>::wxz)                                                                          \
                    .def_prop_ro("wxw", &Vector<T, 4>::wxw)                                                                          \
                    .def_prop_ro("wyx", &Vector<T, 4>::wyx)                                                                          \
                    .def_prop_ro("wyy", &Vector<T, 4>::wyy)                                                                          \
                    .def_prop_ro("wyz", &Vector<T, 4>::wyz)                                                                          \
                    .def_prop_ro("wyw", &Vector<T, 4>::wyw)                                                                          \
                    .def_prop_ro("wzx", &Vector<T, 4>::wzx)                                                                          \
                    .def_prop_ro("wzy", &Vector<T, 4>::wzy)                                                                          \
                    .def_prop_ro("wzz", &Vector<T, 4>::wzz)                                                                          \
                    .def_prop_ro("wzw", &Vector<T, 4>::wzw)                                                                          \
                    .def_prop_ro("wwx", &Vector<T, 4>::wwx)                                                                          \
                    .def_prop_ro("wwy", &Vector<T, 4>::wwy)                                                                          \
                    .def_prop_ro("wwz", &Vector<T, 4>::wwz)                                                                          \
                    .def_prop_ro("www", &Vector<T, 4>::www)                                                                          \
                    .def_prop_ro("xxxx", &Vector<T, 4>::xxxx)                                                                        \
                    .def_prop_ro("xxxy", &Vector<T, 4>::xxxy)                                                                        \
                    .def_prop_ro("xxxz", &Vector<T, 4>::xxxz)                                                                        \
                    .def_prop_ro("xxxw", &Vector<T, 4>::xxxw)                                                                        \
                    .def_prop_ro("xxyx", &Vector<T, 4>::xxyx)                                                                        \
                    .def_prop_ro("xxyy", &Vector<T, 4>::xxyy)                                                                        \
                    .def_prop_ro("xxyz", &Vector<T, 4>::xxyz)                                                                        \
                    .def_prop_ro("xxyw", &Vector<T, 4>::xxyw)                                                                        \
                    .def_prop_ro("xxzx", &Vector<T, 4>::xxzx)                                                                        \
                    .def_prop_ro("xxzy", &Vector<T, 4>::xxzy)                                                                        \
                    .def_prop_ro("xxzz", &Vector<T, 4>::xxzz)                                                                        \
                    .def_prop_ro("xxzw", &Vector<T, 4>::xxzw)                                                                        \
                    .def_prop_ro("xxwx", &Vector<T, 4>::xxwx)                                                                        \
                    .def_prop_ro("xxwy", &Vector<T, 4>::xxwy)                                                                        \
                    .def_prop_ro("xxwz", &Vector<T, 4>::xxwz)                                                                        \
                    .def_prop_ro("xxww", &Vector<T, 4>::xxww)                                                                        \
                    .def_prop_ro("xyxx", &Vector<T, 4>::xyxx)                                                                        \
                    .def_prop_ro("xyxy", &Vector<T, 4>::xyxy)                                                                        \
                    .def_prop_ro("xyxz", &Vector<T, 4>::xyxz)                                                                        \
                    .def_prop_ro("xyxw", &Vector<T, 4>::xyxw)                                                                        \
                    .def_prop_ro("xyyx", &Vector<T, 4>::xyyx)                                                                        \
                    .def_prop_ro("xyyy", &Vector<T, 4>::xyyy)                                                                        \
                    .def_prop_ro("xyyz", &Vector<T, 4>::xyyz)                                                                        \
                    .def_prop_ro("xyyw", &Vector<T, 4>::xyyw)                                                                        \
                    .def_prop_ro("xyzx", &Vector<T, 4>::xyzx)                                                                        \
                    .def_prop_ro("xyzy", &Vector<T, 4>::xyzy)                                                                        \
                    .def_prop_ro("xyzz", &Vector<T, 4>::xyzz)                                                                        \
                    .def_prop_ro("xyzw", &Vector<T, 4>::xyzw)                                                                        \
                    .def_prop_ro("xywx", &Vector<T, 4>::xywx)                                                                        \
                    .def_prop_ro("xywy", &Vector<T, 4>::xywy)                                                                        \
                    .def_prop_ro("xywz", &Vector<T, 4>::xywz)                                                                        \
                    .def_prop_ro("xyww", &Vector<T, 4>::xyww)                                                                        \
                    .def_prop_ro("xzxx", &Vector<T, 4>::xzxx)                                                                        \
                    .def_prop_ro("xzxy", &Vector<T, 4>::xzxy)                                                                        \
                    .def_prop_ro("xzxz", &Vector<T, 4>::xzxz)                                                                        \
                    .def_prop_ro("xzxw", &Vector<T, 4>::xzxw)                                                                        \
                    .def_prop_ro("xzyx", &Vector<T, 4>::xzyx)                                                                        \
                    .def_prop_ro("xzyy", &Vector<T, 4>::xzyy)                                                                        \
                    .def_prop_ro("xzyz", &Vector<T, 4>::xzyz)                                                                        \
                    .def_prop_ro("xzyw", &Vector<T, 4>::xzyw)                                                                        \
                    .def_prop_ro("xzzx", &Vector<T, 4>::xzzx)                                                                        \
                    .def_prop_ro("xzzy", &Vector<T, 4>::xzzy)                                                                        \
                    .def_prop_ro("xzzz", &Vector<T, 4>::xzzz)                                                                        \
                    .def_prop_ro("xzzw", &Vector<T, 4>::xzzw)                                                                        \
                    .def_prop_ro("xzwx", &Vector<T, 4>::xzwx)                                                                        \
                    .def_prop_ro("xzwy", &Vector<T, 4>::xzwy)                                                                        \
                    .def_prop_ro("xzwz", &Vector<T, 4>::xzwz)                                                                        \
                    .def_prop_ro("xzww", &Vector<T, 4>::xzww)                                                                        \
                    .def_prop_ro("xwxx", &Vector<T, 4>::xwxx)                                                                        \
                    .def_prop_ro("xwxy", &Vector<T, 4>::xwxy)                                                                        \
                    .def_prop_ro("xwxz", &Vector<T, 4>::xwxz)                                                                        \
                    .def_prop_ro("xwxw", &Vector<T, 4>::xwxw)                                                                        \
                    .def_prop_ro("xwyx", &Vector<T, 4>::xwyx)                                                                        \
                    .def_prop_ro("xwyy", &Vector<T, 4>::xwyy)                                                                        \
                    .def_prop_ro("xwyz", &Vector<T, 4>::xwyz)                                                                        \
                    .def_prop_ro("xwyw", &Vector<T, 4>::xwyw)                                                                        \
                    .def_prop_ro("xwzx", &Vector<T, 4>::xwzx)                                                                        \
                    .def_prop_ro("xwzy", &Vector<T, 4>::xwzy)                                                                        \
                    .def_prop_ro("xwzz", &Vector<T, 4>::xwzz)                                                                        \
                    .def_prop_ro("xwzw", &Vector<T, 4>::xwzw)                                                                        \
                    .def_prop_ro("xwwx", &Vector<T, 4>::xwwx)                                                                        \
                    .def_prop_ro("xwwy", &Vector<T, 4>::xwwy)                                                                        \
                    .def_prop_ro("xwwz", &Vector<T, 4>::xwwz)                                                                        \
                    .def_prop_ro("xwww", &Vector<T, 4>::xwww)                                                                        \
                    .def_prop_ro("yxxx", &Vector<T, 4>::yxxx)                                                                        \
                    .def_prop_ro("yxxy", &Vector<T, 4>::yxxy)                                                                        \
                    .def_prop_ro("yxxz", &Vector<T, 4>::yxxz)                                                                        \
                    .def_prop_ro("yxxw", &Vector<T, 4>::yxxw)                                                                        \
                    .def_prop_ro("yxyx", &Vector<T, 4>::yxyx)                                                                        \
                    .def_prop_ro("yxyy", &Vector<T, 4>::yxyy)                                                                        \
                    .def_prop_ro("yxyz", &Vector<T, 4>::yxyz)                                                                        \
                    .def_prop_ro("yxyw", &Vector<T, 4>::yxyw)                                                                        \
                    .def_prop_ro("yxzx", &Vector<T, 4>::yxzx)                                                                        \
                    .def_prop_ro("yxzy", &Vector<T, 4>::yxzy)                                                                        \
                    .def_prop_ro("yxzz", &Vector<T, 4>::yxzz)                                                                        \
                    .def_prop_ro("yxzw", &Vector<T, 4>::yxzw)                                                                        \
                    .def_prop_ro("yxwx", &Vector<T, 4>::yxwx)                                                                        \
                    .def_prop_ro("yxwy", &Vector<T, 4>::yxwy)                                                                        \
                    .def_prop_ro("yxwz", &Vector<T, 4>::yxwz)                                                                        \
                    .def_prop_ro("yxww", &Vector<T, 4>::yxww)                                                                        \
                    .def_prop_ro("yyxx", &Vector<T, 4>::yyxx)                                                                        \
                    .def_prop_ro("yyxy", &Vector<T, 4>::yyxy)                                                                        \
                    .def_prop_ro("yyxz", &Vector<T, 4>::yyxz)                                                                        \
                    .def_prop_ro("yyxw", &Vector<T, 4>::yyxw)                                                                        \
                    .def_prop_ro("yyyx", &Vector<T, 4>::yyyx)                                                                        \
                    .def_prop_ro("yyyy", &Vector<T, 4>::yyyy)                                                                        \
                    .def_prop_ro("yyyz", &Vector<T, 4>::yyyz)                                                                        \
                    .def_prop_ro("yyyw", &Vector<T, 4>::yyyw)                                                                        \
                    .def_prop_ro("yyzx", &Vector<T, 4>::yyzx)                                                                        \
                    .def_prop_ro("yyzy", &Vector<T, 4>::yyzy)                                                                        \
                    .def_prop_ro("yyzz", &Vector<T, 4>::yyzz)                                                                        \
                    .def_prop_ro("yyzw", &Vector<T, 4>::yyzw)                                                                        \
                    .def_prop_ro("yywx", &Vector<T, 4>::yywx)                                                                        \
                    .def_prop_ro("yywy", &Vector<T, 4>::yywy)                                                                        \
                    .def_prop_ro("yywz", &Vector<T, 4>::yywz)                                                                        \
                    .def_prop_ro("yyww", &Vector<T, 4>::yyww)                                                                        \
                    .def_prop_ro("yzxx", &Vector<T, 4>::yzxx)                                                                        \
                    .def_prop_ro("yzxy", &Vector<T, 4>::yzxy)                                                                        \
                    .def_prop_ro("yzxz", &Vector<T, 4>::yzxz)                                                                        \
                    .def_prop_ro("yzxw", &Vector<T, 4>::yzxw)                                                                        \
                    .def_prop_ro("yzyx", &Vector<T, 4>::yzyx)                                                                        \
                    .def_prop_ro("yzyy", &Vector<T, 4>::yzyy)                                                                        \
                    .def_prop_ro("yzyz", &Vector<T, 4>::yzyz)                                                                        \
                    .def_prop_ro("yzyw", &Vector<T, 4>::yzyw)                                                                        \
                    .def_prop_ro("yzzx", &Vector<T, 4>::yzzx)                                                                        \
                    .def_prop_ro("yzzy", &Vector<T, 4>::yzzy)                                                                        \
                    .def_prop_ro("yzzz", &Vector<T, 4>::yzzz)                                                                        \
                    .def_prop_ro("yzzw", &Vector<T, 4>::yzzw)                                                                        \
                    .def_prop_ro("yzwx", &Vector<T, 4>::yzwx)                                                                        \
                    .def_prop_ro("yzwy", &Vector<T, 4>::yzwy)                                                                        \
                    .def_prop_ro("yzwz", &Vector<T, 4>::yzwz)                                                                        \
                    .def_prop_ro("yzww", &Vector<T, 4>::yzww)                                                                        \
                    .def_prop_ro("ywxx", &Vector<T, 4>::ywxx)                                                                        \
                    .def_prop_ro("ywxy", &Vector<T, 4>::ywxy)                                                                        \
                    .def_prop_ro("ywxz", &Vector<T, 4>::ywxz)                                                                        \
                    .def_prop_ro("ywxw", &Vector<T, 4>::ywxw)                                                                        \
                    .def_prop_ro("ywyx", &Vector<T, 4>::ywyx)                                                                        \
                    .def_prop_ro("ywyy", &Vector<T, 4>::ywyy)                                                                        \
                    .def_prop_ro("ywyz", &Vector<T, 4>::ywyz)                                                                        \
                    .def_prop_ro("ywyw", &Vector<T, 4>::ywyw)                                                                        \
                    .def_prop_ro("ywzx", &Vector<T, 4>::ywzx)                                                                        \
                    .def_prop_ro("ywzy", &Vector<T, 4>::ywzy)                                                                        \
                    .def_prop_ro("ywzz", &Vector<T, 4>::ywzz)                                                                        \
                    .def_prop_ro("ywzw", &Vector<T, 4>::ywzw)                                                                        \
                    .def_prop_ro("ywwx", &Vector<T, 4>::ywwx)                                                                        \
                    .def_prop_ro("ywwy", &Vector<T, 4>::ywwy)                                                                        \
                    .def_prop_ro("ywwz", &Vector<T, 4>::ywwz)                                                                        \
                    .def_prop_ro("ywww", &Vector<T, 4>::ywww)                                                                        \
                    .def_prop_ro("zxxx", &Vector<T, 4>::zxxx)                                                                        \
                    .def_prop_ro("zxxy", &Vector<T, 4>::zxxy)                                                                        \
                    .def_prop_ro("zxxz", &Vector<T, 4>::zxxz)                                                                        \
                    .def_prop_ro("zxxw", &Vector<T, 4>::zxxw)                                                                        \
                    .def_prop_ro("zxyx", &Vector<T, 4>::zxyx)                                                                        \
                    .def_prop_ro("zxyy", &Vector<T, 4>::zxyy)                                                                        \
                    .def_prop_ro("zxyz", &Vector<T, 4>::zxyz)                                                                        \
                    .def_prop_ro("zxyw", &Vector<T, 4>::zxyw)                                                                        \
                    .def_prop_ro("zxzx", &Vector<T, 4>::zxzx)                                                                        \
                    .def_prop_ro("zxzy", &Vector<T, 4>::zxzy)                                                                        \
                    .def_prop_ro("zxzz", &Vector<T, 4>::zxzz)                                                                        \
                    .def_prop_ro("zxzw", &Vector<T, 4>::zxzw)                                                                        \
                    .def_prop_ro("zxwx", &Vector<T, 4>::zxwx)                                                                        \
                    .def_prop_ro("zxwy", &Vector<T, 4>::zxwy)                                                                        \
                    .def_prop_ro("zxwz", &Vector<T, 4>::zxwz)                                                                        \
                    .def_prop_ro("zxww", &Vector<T, 4>::zxww)                                                                        \
                    .def_prop_ro("zyxx", &Vector<T, 4>::zyxx)                                                                        \
                    .def_prop_ro("zyxy", &Vector<T, 4>::zyxy)                                                                        \
                    .def_prop_ro("zyxz", &Vector<T, 4>::zyxz)                                                                        \
                    .def_prop_ro("zyxw", &Vector<T, 4>::zyxw)                                                                        \
                    .def_prop_ro("zyyx", &Vector<T, 4>::zyyx)                                                                        \
                    .def_prop_ro("zyyy", &Vector<T, 4>::zyyy)                                                                        \
                    .def_prop_ro("zyyz", &Vector<T, 4>::zyyz)                                                                        \
                    .def_prop_ro("zyyw", &Vector<T, 4>::zyyw)                                                                        \
                    .def_prop_ro("zyzx", &Vector<T, 4>::zyzx)                                                                        \
                    .def_prop_ro("zyzy", &Vector<T, 4>::zyzy)                                                                        \
                    .def_prop_ro("zyzz", &Vector<T, 4>::zyzz)                                                                        \
                    .def_prop_ro("zyzw", &Vector<T, 4>::zyzw)                                                                        \
                    .def_prop_ro("zywx", &Vector<T, 4>::zywx)                                                                        \
                    .def_prop_ro("zywy", &Vector<T, 4>::zywy)                                                                        \
                    .def_prop_ro("zywz", &Vector<T, 4>::zywz)                                                                        \
                    .def_prop_ro("zyww", &Vector<T, 4>::zyww)                                                                        \
                    .def_prop_ro("zzxx", &Vector<T, 4>::zzxx)                                                                        \
                    .def_prop_ro("zzxy", &Vector<T, 4>::zzxy)                                                                        \
                    .def_prop_ro("zzxz", &Vector<T, 4>::zzxz)                                                                        \
                    .def_prop_ro("zzxw", &Vector<T, 4>::zzxw)                                                                        \
                    .def_prop_ro("zzyx", &Vector<T, 4>::zzyx)                                                                        \
                    .def_prop_ro("zzyy", &Vector<T, 4>::zzyy)                                                                        \
                    .def_prop_ro("zzyz", &Vector<T, 4>::zzyz)                                                                        \
                    .def_prop_ro("zzyw", &Vector<T, 4>::zzyw)                                                                        \
                    .def_prop_ro("zzzx", &Vector<T, 4>::zzzx)                                                                        \
                    .def_prop_ro("zzzy", &Vector<T, 4>::zzzy)                                                                        \
                    .def_prop_ro("zzzz", &Vector<T, 4>::zzzz)                                                                        \
                    .def_prop_ro("zzzw", &Vector<T, 4>::zzzw)                                                                        \
                    .def_prop_ro("zzwx", &Vector<T, 4>::zzwx)                                                                        \
                    .def_prop_ro("zzwy", &Vector<T, 4>::zzwy)                                                                        \
                    .def_prop_ro("zzwz", &Vector<T, 4>::zzwz)                                                                        \
                    .def_prop_ro("zzww", &Vector<T, 4>::zzww)                                                                        \
                    .def_prop_ro("zwxx", &Vector<T, 4>::zwxx)                                                                        \
                    .def_prop_ro("zwxy", &Vector<T, 4>::zwxy)                                                                        \
                    .def_prop_ro("zwxz", &Vector<T, 4>::zwxz)                                                                        \
                    .def_prop_ro("zwxw", &Vector<T, 4>::zwxw)                                                                        \
                    .def_prop_ro("zwyx", &Vector<T, 4>::zwyx)                                                                        \
                    .def_prop_ro("zwyy", &Vector<T, 4>::zwyy)                                                                        \
                    .def_prop_ro("zwyz", &Vector<T, 4>::zwyz)                                                                        \
                    .def_prop_ro("zwyw", &Vector<T, 4>::zwyw)                                                                        \
                    .def_prop_ro("zwzx", &Vector<T, 4>::zwzx)                                                                        \
                    .def_prop_ro("zwzy", &Vector<T, 4>::zwzy)                                                                        \
                    .def_prop_ro("zwzz", &Vector<T, 4>::zwzz)                                                                        \
                    .def_prop_ro("zwzw", &Vector<T, 4>::zwzw)                                                                        \
                    .def_prop_ro("zwwx", &Vector<T, 4>::zwwx)                                                                        \
                    .def_prop_ro("zwwy", &Vector<T, 4>::zwwy)                                                                        \
                    .def_prop_ro("zwwz", &Vector<T, 4>::zwwz)                                                                        \
                    .def_prop_ro("zwww", &Vector<T, 4>::zwww)                                                                        \
                    .def_prop_ro("wxxx", &Vector<T, 4>::wxxx)                                                                        \
                    .def_prop_ro("wxxy", &Vector<T, 4>::wxxy)                                                                        \
                    .def_prop_ro("wxxz", &Vector<T, 4>::wxxz)                                                                        \
                    .def_prop_ro("wxxw", &Vector<T, 4>::wxxw)                                                                        \
                    .def_prop_ro("wxyx", &Vector<T, 4>::wxyx)                                                                        \
                    .def_prop_ro("wxyy", &Vector<T, 4>::wxyy)                                                                        \
                    .def_prop_ro("wxyz", &Vector<T, 4>::wxyz)                                                                        \
                    .def_prop_ro("wxyw", &Vector<T, 4>::wxyw)                                                                        \
                    .def_prop_ro("wxzx", &Vector<T, 4>::wxzx)                                                                        \
                    .def_prop_ro("wxzy", &Vector<T, 4>::wxzy)                                                                        \
                    .def_prop_ro("wxzz", &Vector<T, 4>::wxzz)                                                                        \
                    .def_prop_ro("wxzw", &Vector<T, 4>::wxzw)                                                                        \
                    .def_prop_ro("wxwx", &Vector<T, 4>::wxwx)                                                                        \
                    .def_prop_ro("wxwy", &Vector<T, 4>::wxwy)                                                                        \
                    .def_prop_ro("wxwz", &Vector<T, 4>::wxwz)                                                                        \
                    .def_prop_ro("wxww", &Vector<T, 4>::wxww)                                                                        \
                    .def_prop_ro("wyxx", &Vector<T, 4>::wyxx)                                                                        \
                    .def_prop_ro("wyxy", &Vector<T, 4>::wyxy)                                                                        \
                    .def_prop_ro("wyxz", &Vector<T, 4>::wyxz)                                                                        \
                    .def_prop_ro("wyxw", &Vector<T, 4>::wyxw)                                                                        \
                    .def_prop_ro("wyyx", &Vector<T, 4>::wyyx)                                                                        \
                    .def_prop_ro("wyyy", &Vector<T, 4>::wyyy)                                                                        \
                    .def_prop_ro("wyyz", &Vector<T, 4>::wyyz)                                                                        \
                    .def_prop_ro("wyyw", &Vector<T, 4>::wyyw)                                                                        \
                    .def_prop_ro("wyzx", &Vector<T, 4>::wyzx)                                                                        \
                    .def_prop_ro("wyzy", &Vector<T, 4>::wyzy)                                                                        \
                    .def_prop_ro("wyzz", &Vector<T, 4>::wyzz)                                                                        \
                    .def_prop_ro("wyzw", &Vector<T, 4>::wyzw)                                                                        \
                    .def_prop_ro("wywx", &Vector<T, 4>::wywx)                                                                        \
                    .def_prop_ro("wywy", &Vector<T, 4>::wywy)                                                                        \
                    .def_prop_ro("wywz", &Vector<T, 4>::wywz)                                                                        \
                    .def_prop_ro("wyww", &Vector<T, 4>::wyww)                                                                        \
                    .def_prop_ro("wzxx", &Vector<T, 4>::wzxx)                                                                        \
                    .def_prop_ro("wzxy", &Vector<T, 4>::wzxy)                                                                        \
                    .def_prop_ro("wzxz", &Vector<T, 4>::wzxz)                                                                        \
                    .def_prop_ro("wzxw", &Vector<T, 4>::wzxw)                                                                        \
                    .def_prop_ro("wzyx", &Vector<T, 4>::wzyx)                                                                        \
                    .def_prop_ro("wzyy", &Vector<T, 4>::wzyy)                                                                        \
                    .def_prop_ro("wzyz", &Vector<T, 4>::wzyz)                                                                        \
                    .def_prop_ro("wzyw", &Vector<T, 4>::wzyw)                                                                        \
                    .def_prop_ro("wzzx", &Vector<T, 4>::wzzx)                                                                        \
                    .def_prop_ro("wzzy", &Vector<T, 4>::wzzy)                                                                        \
                    .def_prop_ro("wzzz", &Vector<T, 4>::wzzz)                                                                        \
                    .def_prop_ro("wzzw", &Vector<T, 4>::wzzw)                                                                        \
                    .def_prop_ro("wzwx", &Vector<T, 4>::wzwx)                                                                        \
                    .def_prop_ro("wzwy", &Vector<T, 4>::wzwy)                                                                        \
                    .def_prop_ro("wzwz", &Vector<T, 4>::wzwz)                                                                        \
                    .def_prop_ro("wzww", &Vector<T, 4>::wzww)                                                                        \
                    .def_prop_ro("wwxx", &Vector<T, 4>::wwxx)                                                                        \
                    .def_prop_ro("wwxy", &Vector<T, 4>::wwxy)                                                                        \
                    .def_prop_ro("wwxz", &Vector<T, 4>::wwxz)                                                                        \
                    .def_prop_ro("wwxw", &Vector<T, 4>::wwxw)                                                                        \
                    .def_prop_ro("wwyx", &Vector<T, 4>::wwyx)                                                                        \
                    .def_prop_ro("wwyy", &Vector<T, 4>::wwyy)                                                                        \
                    .def_prop_ro("wwyz", &Vector<T, 4>::wwyz)                                                                        \
                    .def_prop_ro("wwyw", &Vector<T, 4>::wwyw)                                                                        \
                    .def_prop_ro("wwzx", &Vector<T, 4>::wwzx)                                                                        \
                    .def_prop_ro("wwzy", &Vector<T, 4>::wwzy)                                                                        \
                    .def_prop_ro("wwzz", &Vector<T, 4>::wwzz)                                                                        \
                    .def_prop_ro("wwzw", &Vector<T, 4>::wwzw)                                                                        \
                    .def_prop_ro("wwwx", &Vector<T, 4>::wwwx)                                                                        \
                    .def_prop_ro("wwwy", &Vector<T, 4>::wwwy)                                                                        \
                    .def_prop_ro("wwwz", &Vector<T, 4>::wwwz)                                                                        \
                    .def_prop_ro("wwww", &Vector<T, 4>::wwww);                                                                       \
    m.def("make_" #T "4", [](T a) { return make_##T##4(a); });                                                                       \
    m.def("make_" #T "4", [](T a, T b, T c, T d) { return make_##T##4(a, b, c, d); });                                               \
    m.def("make_" #T "4", [](Vector<T, 4> a) { return make_##T##4(a); });

void export_vector4(nb::module_& m)
{
    LUISA_EXPORT_VECTOR4(bool)
    LUISA_EXPORT_VECTOR4(uint)
    LUISA_EXPORT_VECTOR4(int)
    LUISA_EXPORT_VECTOR4(float)
    LUISA_EXPORT_VECTOR4(double)
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
static ModuleRegister _module_register(export_vector4);