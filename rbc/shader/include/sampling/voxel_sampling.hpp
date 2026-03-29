#pragma once
#include <luisa/std.hpp>
#include "procedural_common.hpp"
#include "shadertoy.hpp"
#include <geometry/procedural_types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>

namespace sampling {
using namespace luisa::shader;
static bool _sample_procedural(
    Ray ray,
    uint type_id,
    auto hit,
    auto &rng,
    float &hit_dist,
    ProceduralGeometry &geometry,
    geometry::VoxelSurface voxel_map) {

    float4x4 inst_matrix = g_accel.instance_transform(hit.inst);
    float3 inst_pos = inst_matrix[3].xyz;

    auto inst_local_to_world = float3x3(
        inst_matrix[0].xyz,
        inst_matrix[1].xyz,
        inst_matrix[2].xyz);
    auto inst_world_to_local = inverse(inst_local_to_world);
    float3 ro = ray.origin();
    float3 rd = ray.dir();
    ro -= inst_pos;
    float3 local_ro = inst_world_to_local * ro;
    float3 local_rd = normalize(inst_world_to_local * rd);
    auto aabb = g_buffer_heap.buffer_read<AABB>(voxel_map.aabb_buffer_heap_idx, voxel_map.aabb_buffer_offset + hit.prim);
    float3 box_min(aabb.packed_min);
    float3 box_max(aabb.packed_max);
    float3 box_center = lerp(box_min, box_max, 0.5f);
    float3 box_size = abs(box_max - box_center);
    float3 local_normal;
    auto local_hit_dist = shadertoy::iBox(local_ro - box_center, local_rd, float2(0, PROCEDURAL_TRACE_MAX_DIST), local_normal, box_size);
    if (local_hit_dist > PROCEDURAL_TRACE_CHECK_DIST) {
        return false;
    }
    float3 local_hit_point = local_hit_dist * local_rd + local_ro;
    auto new_hit_dist = distance(inst_local_to_world * local_hit_point, ro);
    if (new_hit_dist >= hit_dist) return false;

    hit_dist = new_hit_dist;
    local_normal = normalize(inst_local_to_world * local_normal);
    geometry.normal[0] = local_normal.x;
    geometry.normal[1] = local_normal.y;
    geometry.normal[2] = local_normal.z;
    geometry.procedural_id.set_id(type_id, voxel_map.mat_buffer_id);
    geometry.procedural_id.mat_offset = voxel_map.mat_buffer_offset;
    geometry.procedural_id.mat_idx = hit.prim;
    return true;
}
}// namespace sampling
