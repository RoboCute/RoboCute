#pragma once
#include <luisa/std.hpp>
#include "procedural_common.hpp"
#include "shadertoy.hpp"
#include <geometry/procedural_types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/volume_heap_extern.hpp>

namespace sampling {
using namespace luisa::shader;

// SDF
static bool _sample_procedural(
    Ray ray,
    uint type_id,
    auto hit,
    auto &rng,
    float &hit_dist,
    ProceduralGeometry &geometry,
    geometry::SDFMap sdf_map) {

    auto sdf_sample = [&](float3 pos) {
        return g_volume_heap.volume_sample(sdf_map.volume_idx, pos * sdf_map.uvw_scale + sdf_map.uvw_offset, Filter::LINEAR_POINT, Address::EDGE).x;
    };

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
    float3 box_normal;
    auto local_hit_dist = shadertoy::iBoxSimple(local_ro, local_rd, float3(0.5f), box_normal);
    if (all(local_hit_dist > PROCEDURAL_TRACE_CHECK_DIST)) {
        return false;
    }

    float3 p = local_ro + local_rd * max(min(local_hit_dist.x, local_hit_dist.y), 0.f);
    // no voxel, consider as box
    if (sdf_map.volume_idx == ~0u) {
        box_normal = normalize(inst_local_to_world * box_normal);
        geometry.normal[0] = box_normal.x;
        geometry.normal[1] = box_normal.y;
        geometry.normal[2] = box_normal.z;
        geometry.procedural_id.set_id(type_id, sdf_map.mat_buffer_id);
        geometry.procedural_id.mat_offset = sdf_map.mat_buffer_offset;
        geometry.procedural_id.mat_idx = hit.prim;
        hit_dist = local_hit_dist.x;
        return true;
    } else {
        float dist = 0;
        auto sdf_normal = [&](float3 p) noexcept {
            if (dist < 1e-5f) {
                return box_normal;
            }
            static constexpr float d = 1e-3f;
            float3 n = 0.f;
            float sdf_center = sdf_sample(p);
            for (uint i = 0; i < 3; i++) {
                float3 inc = p;
                inc[i] += d;
                n[i] = (1.0f / d) * (sdf_sample(inc) - sdf_center);
            }
            return normalize(n);
        };

        float max_dist = abs(local_hit_dist.y - local_hit_dist.x);
        for (uint i = 0; i < sdf_map.sample_count; ++i) {
            float s = sdf_sample(p + dist * local_rd);
            if (s <= 1e-6f) {
                break;
            };
            dist += s;
            if (dist > max_dist) {
                return false;
            }
        }
        float3 local_hit_point = p + dist * local_rd;
        auto new_hit_dist = distance(inst_local_to_world * local_hit_point, ro);

        float3 local_normal = sdf_normal(local_hit_point);
        if (new_hit_dist >= hit_dist) return false;
        hit_dist = new_hit_dist;
        local_normal = normalize(inst_local_to_world * local_normal);
        geometry.normal[0] = local_normal.x;
        geometry.normal[1] = local_normal.y;
        geometry.normal[2] = local_normal.z;
        geometry.procedural_id.set_id(type_id, sdf_map.mat_buffer_id);
        geometry.procedural_id.mat_offset = sdf_map.mat_buffer_offset;
        geometry.procedural_id.mat_idx = hit.prim;
    }
    return true;
}
}// namespace sampling