#pragma once
#include "matrix.h"
#include <luisa/runtime/rtx/aabb.h>

namespace rbc
{
RBC_RUNTIME_API bool frustum_cull(
    luisa::double4x4 const& local_to_world,
    luisa::compute::AABB const& bounding,
    luisa::span<luisa::double4, 6> frustum_planes,
    luisa::double3 frustum_min_point,
    luisa::double3 frustum_max_point,
    luisa::double3 cam_forward,
    luisa::double3 cam_pos
);
}