#include <luisa/std.hpp>
#include <luisa/types/ray.hpp>
#include <geometry/gaussian_probe.hpp>
using namespace luisa::shader;

/// Compute rotated extent of Gaussian based on scale and rotation
[[nodiscard]] float3 compute_gaussian_extent(float3 scale, float4 rotation) {
    // For a Gaussian, the effective extent is approximately 3 * scale
    // which covers ~99.7% of the Gaussian's influence
    float3 base_extent = scale * 3.0f;

    // Rotate the extent using quaternion
    float3 q_vec = float3(rotation.x, rotation.y, rotation.z);
    float q_w = rotation.w;

    // Compute rotation matrix columns (simplified for AABB approximation)
    float3x3 rot_mat;

    float xx = q_vec.x * q_vec.x;
    float yy = q_vec.y * q_vec.y;
    float zz = q_vec.z * q_vec.z;
    float xy = q_vec.x * q_vec.y;
    float xz = q_vec.x * q_vec.z;
    float yz = q_vec.y * q_vec.z;
    float wx = q_w * q_vec.x;
    float wy = q_w * q_vec.y;
    float wz = q_w * q_vec.z;

    rot_mat[0] = float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy));
    rot_mat[1] = float3(2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx));
    rot_mat[2] = float3(2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy));

    // For AABB, take absolute values and compute projected extents
    float3 extent;
    extent.x = abs(rot_mat[0].x) * base_extent.x + abs(rot_mat[1].x) * base_extent.y + abs(rot_mat[2].x) * base_extent.z;
    extent.y = abs(rot_mat[0].y) * base_extent.x + abs(rot_mat[1].y) * base_extent.y + abs(rot_mat[2].y) * base_extent.z;
    extent.z = abs(rot_mat[0].z) * base_extent.x + abs(rot_mat[1].z) * base_extent.y + abs(rot_mat[2].z) * base_extent.z;

    return extent;
}

/// Build AABB from position and extent
[[nodiscard]] AABB build_aabb(float3 position, float3 extent) {
    AABB aabb;
    float3 min_pos = position - extent;
    float3 max_pos = position + extent;

    aabb.packed_min[0] = min_pos.x;
    aabb.packed_min[1] = min_pos.y;
    aabb.packed_min[2] = min_pos.z;

    aabb.packed_max[0] = max_pos.x;
    aabb.packed_max[1] = max_pos.y;
    aabb.packed_max[2] = max_pos.z;

    return aabb;
}
[[nodiscard]] AABB compute_aabb(Buffer<GaussianProbe> &probe_buffer, uint32 probe_idx) {
    GaussianProbe probe = probe_buffer.read(probe_idx);

    // Compute the rotated extent
    float3 extent = compute_gaussian_extent(float3(probe.scale), probe.rotation);

    // Build and return AABB
    return build_aabb(float3(probe.position), extent);
}

/// Kernel entry point for computing Gaussian AABBs
[[kernel_1d(128)]] int kernel(
    Buffer<AABB> &output_buffer,
    Buffer<GaussianProbe> &probe_buffer) {
    uint32 probe_idx = dispatch_id().x;

    AABB aabb = compute_aabb(probe_buffer, probe_idx);
    output_buffer.write(probe_idx, aabb);

    return 0;
}
