#pragma once
#include "matrix.h"
#include <luisa/runtime/rtx/aabb.h>

namespace rbc
{
RBC_RUNTIME_API bool frustum_cull(
    luisa::float4x4 const& local_to_world,
    luisa::compute::AABB const& bounding,
    luisa::span<luisa::float4, 6> frustum_planes,
    luisa::float3 frustum_min_point,
    luisa::float3 frustum_max_point,
    luisa::float3 cam_forward,
    luisa::float3 cam_pos
);
}