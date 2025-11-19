#pragma once

#include <bit>
#include <cmath>
#include <cstring>
#include <algorithm>

#include <luisa/core/mathematics.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/constants.h>

namespace luisa {

[[nodiscard]] inline auto fract(double x) noexcept {
    return x - std::floor(x);
}

/// Convert degree to radian
[[nodiscard]] constexpr double radians(double deg) noexcept { return deg * constants::pi / 180.0; }
/// Convert radian to degree
[[nodiscard]] constexpr double degrees(double rad) noexcept { return rad * constants::inv_pi * 180.0; }

/// Make function apply to floatN element-wise
#define LUISA_MAKE_VECTOR_UNARY_FUNC(func)                                                                \
    [[nodiscard]] inline auto func(double2 v) noexcept { return double2{func(v.x), func(v.y)}; }            \
    [[nodiscard]] inline auto func(double3 v) noexcept { return double3{func(v.x), func(v.y), func(v.z)}; } \
    [[nodiscard]] inline auto func(double4 v) noexcept { return double4{func(v.x), func(v.y), func(v.z), func(v.w)}; }


#undef LUISA_MAKE_VECTOR_UNARY_FUNC

/// Make function apply to floatN element-wise
#define LUISA_MAKE_VECTOR_BINARY_FUNC(func)                                                  \
    template<typename T>                                                                     \
    [[nodiscard]] constexpr auto func(Vector<T, 2> v, Vector<T, 2> u) noexcept {             \
        return Vector<T, 2>{func(v.x, u.x), func(v.y, u.y)};                                 \
    }                                                                                        \
    template<typename T>                                                                     \
    [[nodiscard]] constexpr auto func(Vector<T, 3> v, Vector<T, 3> u) noexcept {             \
        return Vector<T, 3>{func(v.x, u.x), func(v.y, u.y), func(v.z, u.z)};                 \
    }                                                                                        \
    template<typename T>                                                                     \
    [[nodiscard]] constexpr auto func(Vector<T, 4> v, Vector<T, 4> u) noexcept {             \
        return Vector<T, 4>{func(v.x, u.x), func(v.y, u.y), func(v.z, u.z), func(v.w, u.w)}; \
    }

#undef LUISA_MAKE_VECTOR_BINARY_FUNC

[[nodiscard]] inline auto isnan(double x) noexcept {
    return std::isnan(x);
}
[[nodiscard]] inline auto isnan(double2 v) noexcept { return make_bool2(isnan(v.x), isnan(v.y)); }
[[nodiscard]] inline auto isnan(double3 v) noexcept { return make_bool3(isnan(v.x), isnan(v.y), isnan(v.z)); }
[[nodiscard]] inline auto isnan(double4 v) noexcept { return make_bool4(isnan(v.x), isnan(v.y), isnan(v.z), isnan(v.w)); }

[[nodiscard]] inline auto isinf(double x) noexcept {
    return std::isinf(x);
}
[[nodiscard]] inline auto isinf(double2 v) noexcept { return make_bool2(isinf(v.x), isinf(v.y)); }
[[nodiscard]] inline auto isinf(double3 v) noexcept { return make_bool3(isinf(v.x), isinf(v.y), isinf(v.z)); }
[[nodiscard]] inline auto isinf(double4 v) noexcept { return make_bool4(isinf(v.x), isinf(v.y), isinf(v.z), isinf(v.w)); }
// Vector Functions
[[nodiscard]] constexpr auto dot(double2 u, double2 v) noexcept {
    return u.x * v.x + u.y * v.y;
}
[[nodiscard]] constexpr auto dot(double3 u, double3 v) noexcept {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}
[[nodiscard]] constexpr auto dot(double4 u, double4 v) noexcept {
    return u.x * v.x + u.y * v.y + u.z * v.z + u.w * v.w;
}

template<size_t N>
[[nodiscard]] constexpr auto length(Vector<double, N> u) noexcept {
    return sqrt(dot(u, u));
}

template<size_t N>
[[nodiscard]] constexpr auto normalize(Vector<double, N> u) noexcept {
    return u * (1.0 / length(u));
}

template<size_t N>
[[nodiscard]] constexpr auto distance(Vector<double, N> u, Vector<double, N> v) noexcept {
    return length(u - v);
}

[[nodiscard]] constexpr auto cross(double3 u, double3 v) noexcept {
    return double3{u.y * v.z - v.y * u.z,
                  u.z * v.x - v.z * u.x,
                  u.x * v.y - v.x * u.y};
}

[[nodiscard]] constexpr auto sign(double x) noexcept { return x < 0.0 ? -1.0 : 1.0; }

[[nodiscard]] constexpr auto sign(double2 v) noexcept {
    return make_double2(sign(v.x), sign(v.y));
}

[[nodiscard]] constexpr auto sign(double3 v) noexcept {
    return make_double3(sign(v.x), sign(v.y), sign(v.z));
}

[[nodiscard]] constexpr auto sign(double4 v) noexcept {
    return make_double4(sign(v.x), sign(v.y), sign(v.z), sign(v.w));
}

}// namespace luisa

