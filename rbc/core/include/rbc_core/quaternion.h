#pragma once

#include <luisa/core/basic_types.h>
#include <luisa/core/mathematics.h>
#include <rbc_config.h>
namespace rbc {
using namespace luisa;
struct Quaternion {
    float4 v;
    constexpr Quaternion() noexcept : v{0.0f, 0.0f, 0.0f, 1.0f} {};
    constexpr Quaternion(float4 v) noexcept
        : v{v} {
    }
    constexpr Quaternion(float3 v, float w) noexcept
        : v{v.x, v.y, v.z, w} {
    }
    static Quaternion Identity() { return {}; }

    constexpr auto operator+(Quaternion rhs) const noexcept {
        return Quaternion{v + rhs.v};
    }
    constexpr auto operator-(Quaternion rhs) const noexcept {
        return Quaternion{v - rhs.v};
    }
    constexpr auto operator*(float s) const noexcept {
        return Quaternion{v * s};
    }
    constexpr auto operator/(float s) const noexcept {
        return Quaternion{v / s};
    }
};
struct DecomposedTransform {
    float3 scaling;
    Quaternion quaternion;
    float3 translation;
};
struct DecomposedTransformDouble {
    double3 scaling;
    Quaternion quaternion;
    double3 translation;
};

struct DualQuaternion {
    float4 rotation_quaternion;
    float4 translation_quaternion;
};

[[nodiscard]] RBC_CORE_API DecomposedTransform decompose(float4x4 m) noexcept;
[[nodiscard]] RBC_CORE_API DecomposedTransformDouble decompose(double4x4 const &m) noexcept;
[[nodiscard]] RBC_CORE_API Quaternion quaternion(float3x3 m) noexcept;
[[nodiscard]] RBC_CORE_API Quaternion quaternion(double3x3 const &m) noexcept;
[[nodiscard]] RBC_CORE_API float4x4 rotation(float3 pos, Quaternion rot, float3 local_scale) noexcept;
[[nodiscard]] RBC_CORE_API double4x4 rotation(double3 pos, Quaternion rot, double3 local_scale) noexcept;
[[nodiscard]] RBC_CORE_API float dot(Quaternion q1, Quaternion q2) noexcept;
[[nodiscard]] RBC_CORE_API float length(Quaternion q) noexcept;
[[nodiscard]] RBC_CORE_API float angle_between(Quaternion q1, Quaternion q2) noexcept;
[[nodiscard]] RBC_CORE_API Quaternion normalize(Quaternion q) noexcept;
[[nodiscard]] RBC_CORE_API Quaternion slerp(Quaternion q1, Quaternion q2, float t) noexcept;

// Dual Quaternion
[[nodiscard]] RBC_CORE_API DualQuaternion encode_dual_quaternion(
    float3 position,
    Quaternion rotation) noexcept;

[[nodiscard]] RBC_CORE_API std::pair<float3, Quaternion> decode_dual_quaternion(
    DualQuaternion dual_quaternion) noexcept;

}// namespace rbc