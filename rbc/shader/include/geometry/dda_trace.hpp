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

struct DDAResult {
    float2 hitUV;    // 命中点 UV 坐标
    float hitHeight; // 命中点高度
    float travelDist;// 行进距离
    int steps;       // 实际步数
    bool hit;        // 是否命中
};

// ============================================
// DDA 2D 网格遍历算法 + 高度图相交检测
// ============================================
// RayMarchHeightmapDDA(
//     Texture2D heightmap,        // 高度图纹理 (R通道存储高度 0-1)
//     SamplerState samplerState,  // 采样器
//     float2 grid_size,      // 高度图世界尺寸 (worldSizeX, worldSizeZ)
//     float heightScale,          // 高度缩放系数 (将0-1映射到实际高度)
//     Ray2D ray,                  // 射线定义
//     float maxDist,              // 最大检测距离
//     int maxSteps,               // 最大步进次数
//     float surfaceOffset         // 表面偏移（避免自相交）
// )
static DDAResult ddaTerrainRaycast(
    float2 origin,// 射线起点 (XZ平面)
    float grid_size,
    float2 direction,// 射线方向 (XZ平面，需归一化)
    float ray_height,// 起点高度 (Y轴)
    float ray_slope, // 高度变化率 (dy / sqrt(dx^2 + dz^2))
    BindlessImage &heap,
    uint heap_idx,
    float2 uv_scale,
    float2 uv_offset,
    float2 height_min_max)

{
    direction = normalize(direction);
    DDAResult result;
    result.hit = false;
    float height_range = max(1e-3f, height_min_max.y - height_min_max.x);
    auto sample_tex = [&](float2 uv) {
        float h = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x;
        return ((h - height_min_max.x) / height_range) * grid_size;
    };
    // 垂直
    if (length(direction) < 1e-3f) {
        if (ray_slope > 0) {
            return result;
        }
        auto tex_height = sample_tex(origin / grid_size);
        // TODO height compare
    }
    // --- 1. 初始化 DDA ---
    // 将射线原点转换到高度图 UV 空间 [0,1]
    float2 pos = origin / grid_size;

    // 确保方向归一化并计算 UV 空间的方向
    float2 dir = direction / grid_size;
    float dirLen = length(dir);

    // 如果方向长度接近0，返回未命中
    if (dirLen < 1e-6)
        return result;

    // 归一化 UV 方向
    dir = normalize(dir);

    // 当前 UV 坐标和网格单元坐标
    float2 currentUV = pos;
    int2 mapPos = int2(floor(pos * grid_size));// 世界空间网格坐标

    // DDA 步进参数
    float2 deltaDist = abs(float2(length(dir), length(dir)) / (float2(sign(dir)) * (abs(dir) + 1e-5f)));// 到下一个边界的距离系数
    int2 stepDir = int2(sign(dir.x), sign(dir.y));                                                      // 步进方向 (+1 或 -1)

    // 计算到第一个边界面的距离
    float2 sideDist;
    if (dir.x > 0)
        sideDist.x = (ceil(pos.x) - pos.x) * deltaDist.x;
    else
        sideDist.x = (pos.x - floor(pos.x)) * deltaDist.x;

    if (dir.y > 0)
        sideDist.y = (ceil(pos.y) - pos.y) * deltaDist.y;
    else
        sideDist.y = (pos.y - floor(pos.y)) * deltaDist.y;

    // 确保 sideDist 为正数且基于 UV 单元格大小
    float2 unit = 1.0 / grid_size;// 单个 UV 单元格的世界尺寸
    sideDist = abs(float2(
        (stepDir.x > 0 ? (ceil(pos.x * grid_size) / grid_size - pos.x) : (pos.x - floor(pos.x * grid_size) / grid_size)) / abs(dir.x),
        (stepDir.y > 0 ? (ceil(pos.y * grid_size) / grid_size - pos.y) : (pos.y - floor(pos.y * grid_size) / grid_size)) / abs(dir.y)));

    deltaDist = float2(
        abs(1.0 / (dir.x * grid_size)),
        abs(1.0 / (dir.y * grid_size)));

    // --- 2. DDA 步进循环 ---
    float currentDist = 0.0;
    int side = 0;// 0 = x, 1 = y
    uint end = uint(2 * grid_size);
    for (int i = 0; i < end; i++) {
        // 记录步进前位置用于插值
        float2 prevUV = currentUV;
        float prevDist = currentDist;

        // 执行 DDA 步进
        if (sideDist.x < sideDist.y) {
            sideDist.x += deltaDist.x;
            currentUV.x += stepDir.x / grid_size;
            currentDist = sideDist.x - deltaDist.x;
            side = 0;
        } else {
            sideDist.y += deltaDist.y;
            currentUV.y += stepDir.y / grid_size;
            currentDist = sideDist.y - deltaDist.y;
            side = 1;
        }

        // 边界检查
        if (any(currentUV < 0.0f) || any(currentUV > 1.0f))
            break;

        // --- 3. 高度相交检测 ---
        // 在当前步进点采样高度图
        float terrainHeight = sample_tex(currentUV);

        // 计算射线在当前水平距离处的理论高度
        float2 worldPos = currentUV * grid_size;
        float horizontalDist = length(worldPos - origin);
        float rayHeight = ray_height + ray_slope * horizontalDist;

        // 检查是否低于地形（相交）
        float surfaceOffset = 0;
        if (rayHeight < terrainHeight + surfaceOffset) {
            // --- 4. 精细插值（可选但推荐） ---
            // 在前后两点之间线性插值找到精确交点
            float2 prevWorldPos = prevUV * grid_size;
            float prevHorizontalDist = length(prevWorldPos - origin);
            float prevRayHeight = ray_height + ray_slope * prevHorizontalDist;
            float prevTerrainHeight = sample_tex(prevUV);

            // 如果之前在地面上方，现在在地表下方，进行线性插值
            if (prevRayHeight >= prevTerrainHeight) {
                float t = (prevTerrainHeight - prevRayHeight) /
                          ((rayHeight - terrainHeight) - (prevRayHeight - prevTerrainHeight) + 1e-6);
                t = saturate(t);

                result.hitUV = lerp(prevUV, currentUV, t);
                result.hitHeight = lerp(prevTerrainHeight, terrainHeight, t);
                result.travelDist = lerp(prevHorizontalDist, horizontalDist, t);
            } else {
                // 起点已在地下，直接返回当前点
                result.hitUV = currentUV;
                result.hitHeight = terrainHeight;
                result.travelDist = horizontalDist;
            }

            result.hit = true;
            result.steps = i;
            return result;
        }
    }

    return result;
}

// static bool ddaTerrainRaycast(
//     float3 origin,
//     float grid_size,
//     float3 dir,
//     BindlessImage &heap,
//     uint heap_idx,
//     float2 uv_scale,
//     float2 uv_offset,
//     float2 height_min_max,

//     float3 &out_pos) {
//     dir = normalize(dir);
//     // Project 3D ray to 2D plane for DDA
//     float2 dir_xz = dir.xz;

//     float height_range = max(1e-3f, height_min_max.y - height_min_max.x);
//     auto sample_tex = [&](float2 uv) {
//         float h = heap.image_sample(heap_idx, uv * uv_scale + uv_offset, Filter::POINT, Address::EDGE).x;
//         return (h - height_min_max.x) / height_range;
//     };
//     // Handle horizontal ray case
//     float len_xz = length(dir_xz);
//     if (len_xz < 0.001f) {
//         if (dir.y >= 0) return false;
//         // Vertical ray, sample directly
//         float2 uv = origin.xz;
//         float h = sample_tex(uv);
//         out_pos = origin;
//         return true;
//     }
//     auto normalized_dir_xz = normalize(dir_xz);
//     auto grid_origin = origin.xz * grid_size;
//     // Standard 2D DDA setup
//     float2 delta = abs(1.0f / (float2(sign(normalized_dir_xz)) * max(abs(normalized_dir_xz), float2(1e-5f))));
//     float2 step = sign(normalized_dir_xz);
//     float2 mapPos = floor(grid_origin);
//     float2 sideDist =
//         (step * (mapPos - grid_origin) + step * 0.5f + 0.5f) * delta;

//     float t = 0.0f;
//     float2 grid_pos = grid_origin;
//     uint iter_count = uint(grid_size * 2 + 0.1) - 1;
//     for (int i = 0; i < iter_count; i++) {
//         // Calculate current 3D position
//         grid_pos = grid_origin + normalized_dir_xz * t;
//         float nrd_t = dda_get_t(grid_origin, grid_pos, grid_size, len_xz);
//         float3 pos_3d = origin + dir * nrd_t;
//         if (grid_pos.y <= 0) {
//             auto origin_xz = grid_pos / grid_size;
//             out_pos = pos_3d;
//             return true;
//         }

//         // Sample heightmap
//         float2 uv = mapPos / grid_size;
//         if (any(uv < 1e-5f || uv >= 0.9999f)) return false;
//         float img_height = sample_tex(uv);
//         // Check if hit terrain
//         if (pos_3d.y <= img_height) {
//             // Refine hit point (optional)
//             out_pos = pos_3d;
//             return true;
//         }

//         // DDA step to next grid
//         if (sideDist.x < sideDist.y) {
//             t = sideDist.x;
//             sideDist.x += delta.x;
//             mapPos.x += step.x;
//         } else {
//             t = sideDist.y;
//             sideDist.y += delta.y;
//             mapPos.y += step.y;
//         }
//     }
//     return false;
// }

}// namespace geometry
