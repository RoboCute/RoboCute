#include <rbc_core/quaternion.h>
#include <luisa/core/logging.h>
#include <rtm/qvvd.h>
#include <rtm/qvvf.h>
namespace rbc {
DecomposedTransform decompose(float4x4 m) noexcept {
    auto t = m[3].xyz();
    auto N = make_float3x3(m);
    auto near_zero = [](auto f) noexcept {
        return std::abs(f) <= 1e-4f;
    };
    auto R = N;
    constexpr auto max_iteration_count = 100u;
    for (auto i = 0u; i < max_iteration_count; i++) {
        auto R_it = inverse(transpose(R));
        auto R_next = 0.5f * (R + R_it);
        auto diff = R - R_next;
        R = R_next;
        auto n = abs(diff[0]) + abs(diff[1]) + abs(diff[2]);
        if (near_zero(std::max({n.x, n.y, n.z}))) { break; }
    }
    auto S = inverse(R) * N;
    if (!near_zero(S[0].y) || !near_zero(S[0].z) ||
        !near_zero(S[1].x) || !near_zero(S[1].z) ||
        !near_zero(S[2].x) || !near_zero(S[2].y)) [[unlikely]] {
        LUISA_WARNING_WITH_LOCATION(
            "Non-zero entries found in decomposed scaling matrix: "
            "(({}, {}, {}), ({}, {}, {}), ({}, {}, {})).",
            S[0].x, S[1].x, S[2].x,
            S[0].y, S[1].y, S[2].y,
            S[0].z, S[1].z, S[2].z);
    }
    auto s = make_float3(S[0].x, S[1].y, S[2].z);
    auto q = quaternion(R);
    return {s, q, t};
}
DecomposedTransformDouble decompose(double4x4 const &m) noexcept {
    auto t = m[3].xyz();
    auto N = make_double3x3(m);
    auto near_zero = [](auto f) noexcept {
        return std::abs(f) <= 1e-5;
    };
    auto R = N;
    constexpr auto max_iteration_count = 100u;
    for (auto i = 0u; i < max_iteration_count; i++) {
        auto R_it = inverse(transpose(R));
        auto R_next = 0.5 * (R + R_it);
        auto diff = R - R_next;
        R = R_next;
        auto n = abs(diff[0]) + abs(diff[1]) + abs(diff[2]);
        if (near_zero(std::max({n.x, n.y, n.z}))) { break; }
    }
    auto S = inverse(R) * N;
    if (!near_zero(S[0].y) || !near_zero(S[0].z) ||
        !near_zero(S[1].x) || !near_zero(S[1].z) ||
        !near_zero(S[2].x) || !near_zero(S[2].y)) [[unlikely]] {
        LUISA_WARNING_WITH_LOCATION(
            "Non-zero entries found in decomposed scaling matrix: "
            "(({}, {}, {}), ({}, {}, {}), ({}, {}, {})).",
            S[0].x, S[1].x, S[2].x,
            S[0].y, S[1].y, S[2].y,
            S[0].z, S[1].z, S[2].z);
    }
    auto s = make_double3(S[0].x, S[1].y, S[2].z);
    auto q = quaternion(R);
    return {s, q, t};
}
static_assert(sizeof(rtm::matrix3x3f) == sizeof(float3x3) && alignof(rtm::matrix3x3f) == alignof(float3x3), "size mismatch.");
static_assert(sizeof(rtm::quatf) == sizeof(float4) && alignof(rtm::quatf) == alignof(float4), "size mismatch.");
static_assert(sizeof(rtm::vector4f) == sizeof(float3) && alignof(rtm::vector4f) == alignof(float3), "size mismatch.");
Quaternion quaternion(float3x3 m) noexcept {
    auto quad = rtm::quat_from_matrix(reinterpret_cast<rtm::matrix3x3f const &>(m));
    return reinterpret_cast<Quaternion const &>(quad);
}
Quaternion quaternion(double3x3 const &m) noexcept {
    auto quad = rtm::quat_from_matrix(reinterpret_cast<rtm::matrix3x3d const &>(m));
    auto quadd = reinterpret_cast<double4 const &>(quad);
    return {make_float4(quadd)};
}

double4x4 rotation(double3 pos, Quaternion rot, double3 local_scale) noexcept {
    double4 quad{
        rot.v.x,
        rot.v.y,
        rot.v.z,
        rot.v.w};
    rtm::qvvd qvv{
        .rotation = reinterpret_cast<rtm::quatd const &>(quad),
        .translation = reinterpret_cast<rtm::vector4d const &>(pos),
        .scale = reinterpret_cast<rtm::vector4d const &>(local_scale)};
    reinterpret_cast<double *>(&qvv.scale)[3] = 0.0;
    reinterpret_cast<double *>(&qvv.translation)[3] = 1.0;
    auto result = rtm::matrix_from_qvv(qvv);
    return make_double4x4(
        reinterpret_cast<double4 const &>(result.x_axis),
        reinterpret_cast<double4 const &>(result.y_axis),
        reinterpret_cast<double4 const &>(result.z_axis),
        reinterpret_cast<double4 const &>(result.w_axis));
}

float4x4 rotation(float3 pos, Quaternion rot, float3 local_scale) noexcept {
    rtm::qvvf qvv{
        .rotation = reinterpret_cast<rtm::quatf const &>(rot),
        .translation = reinterpret_cast<rtm::vector4f const &>(pos),
        .scale = reinterpret_cast<rtm::vector4f const &>(local_scale)};
    reinterpret_cast<float *>(&qvv.translation)[3] = 0.0;
    auto result = rtm::matrix_from_qvv(qvv);
    return make_float4x4(
        reinterpret_cast<float4 const &>(result.x_axis),
        reinterpret_cast<float4 const &>(result.y_axis),
        reinterpret_cast<float4 const &>(result.z_axis),
        reinterpret_cast<float4 const &>(result.w_axis));
}

float angle_between(Quaternion q1, Quaternion q2) noexcept {
    auto q = q1.v - q2.v;
    return rtm::quat_get_angle(
        reinterpret_cast<rtm::quatf const &>(q));
}

Quaternion slerp(Quaternion q1, Quaternion q2, float t) noexcept {
    auto quad = rtm::quat_slerp(
        reinterpret_cast<rtm::quatf const &>(q1),
        reinterpret_cast<rtm::quatf const &>(q2),
        t);
    return reinterpret_cast<Quaternion const &>(quad);
}

float dot(Quaternion q1, Quaternion q2) noexcept {
    return dot(q1.v.xyz(), q2.v.xyz()) + q1.v[3] * q2.v[3];
}

float length(Quaternion q) noexcept {
    return std::sqrt(dot(q, q));
}

Quaternion normalize(Quaternion q) noexcept {
    return q / length(q);
}

DualQuaternion encode_dual_quaternion(
    float3 position,
    Quaternion rotation) noexcept {
    DualQuaternion r;
    r.rotation_quaternion = rotation.v;
    auto &dq = reinterpret_cast<float4 &>(r.translation_quaternion);
    auto const &q0 = reinterpret_cast<float4 &>(rotation);
    dq[0] = -0.5 * (position[0] * q0[1] + position[1] * q0[2] + position[2] * q0[3]);
    dq[1] = 0.5 * (position[0] * q0[0] + position[1] * q0[3] - position[2] * q0[2]);
    dq[2] = 0.5 * (-position[0] * q0[3] + position[1] * q0[0] + position[2] * q0[1]);
    dq[3] = 0.5 * (position[0] * q0[2] - position[1] * q0[1] + position[2] * q0[0]);
    return r;
}

std::pair<float3, Quaternion> decode_dual_quaternion(
    DualQuaternion dual_quaternion) noexcept {
    std::pair<float3, Quaternion> r;
    r.second = dual_quaternion.rotation_quaternion;
    float4 *dq = &dual_quaternion.rotation_quaternion;
    auto &t = r.first;
    t[0] = 2.0 * (-dq[1][0] * dq[0][1] + dq[1][1] * dq[0][0] - dq[1][2] * dq[0][3] + dq[1][3] * dq[0][2]);
    t[1] = 2.0 * (-dq[1][0] * dq[0][2] + dq[1][1] * dq[0][3] + dq[1][2] * dq[0][0] - dq[1][3] * dq[0][1]);
    t[2] = 2.0 * (-dq[1][0] * dq[0][3] - dq[1][1] * dq[0][2] + dq[1][2] * dq[0][1] + dq[1][3] * dq[0][0]);
    return r;
}

}// namespace rbc