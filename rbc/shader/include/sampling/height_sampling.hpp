#pragma once
#include <luisa/std.hpp>
#include "procedural_common.hpp"
#include "shadertoy.hpp"
#include <geometry/procedural_types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/volume_heap_extern.hpp>
#include <geometry/gaussian_probe.hpp>
#include <geometry/dual_quaternion.hpp>
#include <geometry/dda_trace.hpp>

#include <luisa/resources/image_heap_extern.hpp>

namespace sampling {
using namespace luisa::shader;

static bool _sample_procedural(
    Ray ray,
    uint type_id,
    auto hit,
    auto &rng,
    float &hit_dist,
    ProceduralGeometry &geometry,
    geometry::HeightMap height) {
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
    float3 local_ro = (inst_world_to_local * ro);
    float3 local_rd = normalize(inst_world_to_local * rd);

    uint2 block_coord = uint2(hit.prim % height.block_size.x, hit.prim / height.block_size.y);
    auto aabb = g_buffer_heap.buffer_read<AABB>(height.aabb_buffer_heap_idx, hit.prim);
    float3 box_min(aabb.packed_min);
    float3 box_max(aabb.packed_max);
    float3 box_center = lerp(box_min, box_max, 0.5f);
    float3 box_size = abs(box_max - box_center);
    float2 local_hit_t;
    float3 box_normal;
    local_ro -= box_center;
    shadertoy::iBox(local_ro, local_rd, float2(0, PROCEDURAL_TRACE_MAX_DIST), box_size, local_hit_t, box_normal);
    // Not hit box
    if (all(local_hit_t >= PROCEDURAL_TRACE_MAX_DIST)) {
        return false;
    }

    float height_max = g_buffer_heap.byte_buffer_read<float>(height.height_minmax_buffer_idx, height.height_minmax_buffer_offset_bytes + hit.prim * sizeof(float));
    float3 hit_start_pos = (local_ro + local_rd * max(local_hit_t.x, ray.t_min)) / box_size;
    float3 hit_end_pos = (local_ro + local_rd * local_hit_t.y) / box_size;
    hit_start_pos = ite(hit_start_pos > 0.999f, float3(1.0), hit_start_pos);
    hit_start_pos = ite(hit_start_pos < -0.999f, float3(-1.0), hit_start_pos);
    hit_end_pos = ite(hit_end_pos > 0.999f, float3(1.0), hit_end_pos);
    hit_end_pos = ite(hit_end_pos < -0.999f, float3(-1.0), hit_end_pos);

    // To UVW
    hit_start_pos = saturate(hit_start_pos * 0.5f + 0.5f);
    hit_end_pos = saturate(hit_end_pos * 0.5f + 0.5f);
    auto hit_dir = normalize(hit_end_pos - hit_start_pos);
    if (dot(hit_dir, ray.dir()) < 0) {
        auto temp = hit_start_pos;
        hit_start_pos = hit_end_pos;
        hit_end_pos = temp;
        hit_dir = -hit_dir;
    }

    float3 box_local_pos;
    const float grid_size = 32.f;
    geometry::DDAResult dda_result = geometry::ddaTerrainRaycast(
        hit_start_pos.xz * grid_size,
        grid_size,// one grid
        hit_dir.xz,
        hit_start_pos.y * grid_size,
        hit_dir.y / sqrt(max(hit_dir.x * hit_dir.x + hit_dir.z * hit_dir.z, 1e-4)),
        g_image_heap,
        height.heightmap_idx,
        1.0f / float2(height.block_size),
        float2(block_coord) / float2(height.block_size),
        float2(0, height_max));

    if (!dda_result.hit) return false;

    dda_result.travelDist = max(dda_result.travelDist, 0.f);
    box_local_pos = hit_start_pos + hit_dir * dda_result.travelDist;
    auto new_hit_dist = distance(inst_local_to_world * ((box_local_pos * 2.0f - 1.0f) * box_size + box_center), ro);
    if (new_hit_dist >= hit_dist) return false;
    hit_dist = new_hit_dist;
    ///////////////// Sobel calculate normal
    float3 local_normal;
    if (dda_result.travelDist < 1e-4) {
        local_normal = box_normal;
    } else {
        // Compute UV from DDA return value (xz plane)
        float2 uv_scale = 1.0f / float2(height.block_size);
        float2 uv_offset = float2(block_coord) / float2(height.block_size);
        float2 uv = box_local_pos.xz * uv_scale + uv_offset;

        // Sample heightmap and compute normal using Sobel operator
        // Get texture size for computing pixel offset
        uint2 tex_size = g_image_heap.image_size(height.heightmap_idx);
        float2 texel_size = 1.0f / float2(tex_size);

        // Helper to sample height at offset
        auto sample_height = [&](float2 offset) -> float {
            float2 sample_uv = uv + offset * texel_size;
            return g_image_heap.image_sample(height.heightmap_idx, sample_uv, Filter::POINT, Address::EDGE).x;
        };

        // Sample 3x3 neighborhood for Sobel operator
        float tl = sample_height(float2(-1.0f, -1.0f));
        float t = sample_height(float2(0.0f, -1.0f));
        float tr = sample_height(float2(1.0f, -1.0f));
        float l = sample_height(float2(-1.0f, 0.0f));
        float r = sample_height(float2(1.0f, 0.0f));
        float bl = sample_height(float2(-1.0f, 1.0f));
        float b = sample_height(float2(0.0f, 1.0f));
        float br = sample_height(float2(1.0f, 1.0f));

        // Sobel gradients
        float dx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
        float dy = (bl + 2.0f * b + br) - (tl + 2.0f * t + tr);

        dx *= tex_size.x;
        dy *= tex_size.y;
        // Construct normal (up is positive Y for heightmap)
        // local_normal = float3(-dx * 0.25f, 1.f, -dy * 0.25f);
        local_normal = float3(-dx, 1.f, -dy);
        // Transform normal from local space to world space
        local_normal = normalize(local_normal);
    }

    local_normal = normalize(inst_local_to_world * local_normal);
    geometry.normal[0] = local_normal.x;
    geometry.normal[1] = local_normal.y;
    geometry.normal[2] = local_normal.z;

    // Set procedural_id like other types
    geometry.procedural_id.set_id(type_id, height.mat_buffer_id);
    geometry.procedural_id.mat_offset = 0;// HeightMap doesn't use mat_offset like SDF
    geometry.procedural_id.mat_idx = hit.prim;
    return true;
}

}