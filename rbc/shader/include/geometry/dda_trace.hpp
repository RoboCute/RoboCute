#pragma once

#include <luisa/std.hpp>

namespace geometry {
using namespace luisa::shader;

// 3D DDA for heightfield - used for terrain collision detection
static bool ddaTerrainRaycast(
    float3 origin,
    float3 dir,
    BindlessImage& heap,
    uint heap_idx,
    float2 uv_scale,
    float2 uv_offset,
    float height_scale,
    float height_offset,

    float3 &out_pos) {
    dir = normalize(dir);
    // Project 3D ray to 2D plane for DDA
    float2 rayPos2D = origin.xz;
    float2 rayDir2D = dir.xz;

    // Handle horizontal ray case
    float len_xz = length(rayDir2D);
    if (len_xz < 0.001f) {
        // Vertical ray, sample directly
        float2 uv = origin.xz;
        if (dir.y > 0) return false;
        float h = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x * height_scale + height_offset;
        out_pos = float3(origin.x, h, origin.z);
        return true;
    }
    // Standard 2D DDA setup
    float2 delta = abs(1.0f / rayDir2D);
    float2 step = sign(rayDir2D);
    float2 mapPos = floor(rayPos2D);
    float2 sideDist =
        (step * (mapPos - rayPos2D) + step * 0.5f + 0.5f) * delta;

    float t = 0.0f;
    float3 currentPos = origin;
    float2 heightmapSize = float2(heap.image_size(heap_idx));

    for (int i = 0; i < 256; i++) {
        // Calculate current 3D position
        currentPos = origin + dir * t;
        if (currentPos.y <= 0) {
            out_pos = float3(currentPos.x, 0, currentPos.z);
            return true;
        }

        // Sample heightmap
        float2 uv = mapPos / heightmapSize;
        if (any(uv < 0.f || uv > 1.0f)) return false;
        float terrainHeight = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x * height_scale + height_offset;

        // Check if hit terrain
        if (currentPos.y <= terrainHeight) {
            // Refine hit point (optional)
            out_pos = float3(currentPos.x, terrainHeight, currentPos.z);
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

        // Calculate new height and check if passed through terrain
        float nextHeight = (origin + dir * t).y;
        if (nextHeight <= terrainHeight && currentPos.y > terrainHeight) {
            // Linear interpolation for precise hit point
            float alpha = (terrainHeight - currentPos.y) / (nextHeight - currentPos.y);
            out_pos = lerp(currentPos, origin + dir * t, alpha);
            return true;
        }
    }
    return false;
}

}// namespace geometry
