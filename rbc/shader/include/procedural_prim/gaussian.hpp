#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
#include <luisa/types/ray.hpp>
using namespace luisa::shader;
#endif

/// Gaussian probe data structure
struct GaussianProbe {
    float4 rotation;// quaternion (x, y, z, w)
    std::array<float, 3> position;
    float opacity;
    std::array<float, 3> scale;
};

/// Compute rotated extent of Gaussian based on scale and rotation
/// Uses the fact that for AABB, we only need sum of absolute rotation matrix columns
[[nodiscard]] inline float3 compute_gaussian_extent(float3 scale, float4 q) {
    // Quaternion is stored as (x, y, z, w) in float4
    // Extract vector part (x, y, z) and scalar part w directly
    float3 v = q.xyz;
    float w = q.w;

    // Base extent: 3 sigma covers ~99.7% of Gaussian influence
    float3 e = scale * 3.0f;

    // Precompute squares and products (used in rotation matrix)
    float3 vv = v * v;
    float xx = vv.x, yy = vv.y, zz = vv.z;
    float xy = v.x * v.y;
    float xz = v.x * v.z;
    float yz = v.y * v.z;
    float wx = w * v.x;
    float wy = w * v.y;
    float wz = w * v.z;

    // Compute absolute values of rotation matrix columns directly
    // For AABB extent, we need |R| * e where |R| is element-wise absolute rotation matrix
    // Column 0: (1-2(yy+zz), 2(xy+wz), 2(xz-wy))
    // Column 1: (2(xy-wz), 1-2(xx+zz), 2(yz+wx))
    // Column 2: (2(xz+wy), 2(yz-wx), 1-2(xx+yy))
    float3 col0 = abs(float3(
        1.0f - 2.0f * (yy + zz),
        2.0f * (xy + wz),
        2.0f * (xz - wy)));
    float3 col1 = abs(float3(
        2.0f * (xy - wz),
        1.0f - 2.0f * (xx + zz),
        2.0f * (yz + wx)));
    float3 col2 = abs(float3(
        2.0f * (xz + wy),
        2.0f * (yz - wx),
        1.0f - 2.0f * (xx + yy)));

    // AABB extent = sum of absolute column vectors weighted by base extent
    return col0 * e.x + col1 * e.y + col2 * e.z;
}

/// Build AABB from position and extent
[[nodiscard]] inline AABB build_aabb(float3 position, float3 extent) {
    AABB aabb;
    float3 min_pos = position - extent * 3.f;
    float3 max_pos = position + extent * 3.f;

    aabb.packed_min[0] = min_pos.x;
    aabb.packed_min[1] = min_pos.y;
    aabb.packed_min[2] = min_pos.z;

    aabb.packed_max[0] = max_pos.x;
    aabb.packed_max[1] = max_pos.y;
    aabb.packed_max[2] = max_pos.z;

    return aabb;
}

/// Compute AABB for a Gaussian probe
[[nodiscard]] inline AABB compute_gaussian_aabb(Buffer<GaussianProbe> &probe_buffer, uint32 probe_idx) {
    GaussianProbe probe = probe_buffer.read(probe_idx);

    // Compute the rotated extent
    float3 extent = compute_gaussian_extent(float3(probe.scale), probe.rotation);

    // Build and return AABB
    return build_aabb(float3(probe.position), extent);
}
