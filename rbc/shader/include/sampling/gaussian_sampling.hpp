#pragma once
#include <luisa/std.hpp>
#include "procedural_common.hpp"
#include "shadertoy.hpp"
#include <geometry/procedural_types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/volume_heap_extern.hpp>
#include <geometry/gaussian_probe.hpp>
#include <geometry/dual_quaternion.hpp>

namespace sampling {
using namespace luisa::shader;
template <typename Float4_T, typename Float3x3_T>
inline Float3x3_T R_from_qvec(Float4_T q, bool col_major = true)
{

    // q = (x, y, z, w)
    auto       x = q.x;
    auto       y = q.y;
    auto       z = q.z;
    auto       w = q.w;
    Float3x3_T R;
    // col 1
    R[0][0] = 1 - 2 * y * y - 2 * z * z;
    R[0][1] = 2 * x * y + 2 * z * w;
    R[0][2] = 2 * x * z - 2 * y * w;
    // col 2
    R[1][0] = 2 * x * y - 2 * z * w;
    R[1][1] = 1 - 2 * x * x - 2 * z * z;
    R[1][2] = 2 * y * z + 2 * x * w;
    // col 3
    R[2][0] = 2 * x * z + 2 * y * w;
    R[2][1] = 2 * y * z - 2 * x * w;
    R[2][2] = 1 - 2 * x * x - 2 * y * y;

    return R;
}

float3x3 calc_cov(float3 scale, float4 qvec)
{
    // LuisaCompute is Col-Major
    float3x3 R = R_from_qvec<float4, float3x3>(qvec);
    float3x3 S;
    S[0][0] = scale.x;
    S[1][1] = scale.y;
    S[2][2] = scale.z;
    // compute covariance
    // $\Sigma=RSS^TR^T$
    float3x3 M = R * S;
    return M * transpose(M);
}

static bool _sample_procedural(
    Ray ray,
    uint type_id,
    auto hit,
    auto &rng,
    float &hit_dist,
    ProceduralGeometry &geometry,
    geometry::GaussianSplatingGeometry gs)  {

    float4x4 inst_matrix = g_accel.instance_transform(hit.inst);
    float3 inst_pos = inst_matrix[3].xyz;

    auto inst_local_to_world = float3x3(
        inst_matrix[0].xyz,
        inst_matrix[1].xyz,
        inst_matrix[2].xyz);
    auto inst_world_to_local = inverse(inst_local_to_world);

    GaussianProbe probe = g_buffer_heap.byte_buffer_read<GaussianProbe>(
        gs.buffer_id,
        gs.probe_offset + hit.prim * sizeof(GaussianProbe));
    geometry.procedural_id.set_id(type_id, gs.buffer_id);
    geometry.procedural_id.mat_idx = hit.prim;
    geometry.procedural_id.mat_offset = gs.mat_buffer_offset;
    geometry.procedural_id.sh_degree = gs.sh_degree;

    // Transform ray to instance local space
    float3 ro = ray.origin() - inst_pos;
    float3 rd = ray.dir();
    ro = inst_world_to_local * ro;
    rd = normalize(inst_world_to_local * rd);

    // Transform ray to probe's local space (translate and rotate)
    ro -= float3(probe.position[0], probe.position[1], probe.position[2]);

    // Quaternion stored as (x, y, z, w)
    float4 q = probe.rotation;
    q = q.yzwx; // IMPORTANT: rxyz -> xyzw

    // Apply inverse rotation to ray origin and direction to transform from
    // world space to probe's local space (where ellipsoid is axis-aligned)
    auto rotate_vector = [&](float4 rot, float3 v) -> float3 {
        float4 vq = float4(v.x, v.y, v.z, 0.0f);
        float4 t = geometry::QuaternionMultiply(rot, vq);
        t = geometry::QuaternionMultiply(t, geometry::QuaternionInvert(rot));
        return t.xyz;
    };

    ro = rotate_vector(geometry::QuaternionInvert(q), ro);
    rd = rotate_vector(geometry::QuaternionInvert(q), rd);

    // Intersect with ellipsoid using scale as radius
    float3 scale = float3(probe.scale);
    float3 normal;
    float2 distBound = float2(0.0f, hit_dist);
    float d = shadertoy::iEllipsoid(ro, rd, distBound, normal, scale);

    if (d >= PROCEDURAL_TRACE_MAX_DIST) {
        return false;
    }

    // Calculate world-space hit distance
    float3 local_hit_point = ro + d * rd;
    // Transform back from probe local to instance local (apply forward rotation)
    local_hit_point = rotate_vector(q, local_hit_point);
    local_hit_point += float3(probe.position[0], probe.position[1], probe.position[2]);
    // Transform from instance local to world
    float3 world_hit_point = inst_local_to_world * local_hit_point + inst_pos;
    auto new_hit_dist = distance(world_hit_point, ray.origin());

    if (new_hit_dist >= hit_dist) {
        return false;
    }

    // Transform normal from local space back to world space
    // The normal from iEllipsoid is in the rotated (but not translated) space
    normal = normalize(rotate_vector(q, normal));
    geometry.normal = normalize(inst_local_to_world * normal);
    hit_dist = new_hit_dist;

    return true;
}


}