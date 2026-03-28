#pragma once

#include <luisa/std.hpp>

namespace geometry {
using namespace luisa::shader;

// 3D DDA for heightfield - used for terrain collision detection
static float dda_get_t(
    float2 grid_origin,
    float2 grid_end_pos,
    float grid_size,
    float dir_xz_len) {
    float xz_len = distance(grid_end_pos, grid_origin) / grid_size;
    return xz_len / dir_xz_len;
}
static bool ddaTerrainRaycast(
    float3 origin,
    float grid_size,
    float3 dir,
    BindlessImage &heap,
    uint heap_idx,
    float2 uv_scale,
    float2 uv_offset,
    float height_scale,
    float height_offset,

    float3 &out_pos) {
    dir = normalize(dir);
    // Project 3D ray to 2D plane for DDA
    float2 dir_xz = dir.xz;

    // Handle horizontal ray case
    float len_xz = length(dir_xz);
    if (dir.y >= 0) {
        float2 uv = origin.xz;
        float h = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x * height_scale + height_offset;
        if (h >= origin.y) {
            out_pos = origin;
            return true;
        }
        return false;
    }
    if (len_xz < 0.001f) {
        // Vertical ray, sample directly
        float2 uv = origin.xz;
        float h = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x * height_scale + height_offset;
        out_pos = origin;
        return true;
    }
    auto normalized_dir_xz = normalize(dir_xz);
    auto grid_origin = origin.xz * grid_size;
    // Standard 2D DDA setup
    float2 delta = abs(1.0f / normalized_dir_xz);
    float2 step = sign(normalized_dir_xz);
    float2 mapPos = floor(grid_origin);
    float2 sideDist =
        (step * (mapPos - grid_origin) + step * 0.5f + 0.5f) * delta;

    float t = 0.0f;
    float2 grid_pos = grid_origin;
    uint iter_count = uint(grid_size * 2 + 0.1) - 1;
    for (int i = 0; i < iter_count; i++) {
        // Calculate current 3D position
        grid_pos = grid_origin + normalized_dir_xz * t;
        float nrd_t = dda_get_t(grid_origin, grid_pos, grid_size, len_xz);
        float3 pos_3d = origin + dir * nrd_t;
        if (grid_pos.y <= 0) {
            auto origin_xz = grid_pos / grid_size;
            out_pos = pos_3d;
            return true;
        }

        // Sample heightmap
        float2 uv = mapPos / grid_size;
        if (any(uv < 0.f || uv > 1.0f)) return false;
        float img_height = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x * height_scale + height_offset;

        // Check if hit terrain
        if (pos_3d.y <= img_height) {
            // Refine hit point (optional)
            out_pos = pos_3d;
            return true;
        }

        // DDA step to next grid
        if (sideDist.x < sideDist.y) {
            t = sideDist.x;
            sideDist.x += delta.x;
            mapPos.x += step.x;
        } else {
            t = sideDist.y;
            sideDist.y += delta.y;
            mapPos.y += step.y;
        }
    }
    return false;
}

}// namespace geometry
