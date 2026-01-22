#pragma once
#include <luisa/core/stl/vector.h>
#include <luisa/core/mathematics.h>
#include <rbc_core/type_info.h>
namespace rbc {
using namespace luisa;
struct GridDrawCmd {
    float3 grid_tangent{1, 0, 0};
    float3 grid_bitangent{0, 0, 1};
    float3 grid_center;
    uint2 line_count{100, 100};
    float2 interval_size{1, 1};
    float4 origin_color{1, 1, 1, 0.5f};
    float start_decay_dist{50};
    float decay_distance{50};
};
struct GridDrawer {
    luisa::vector<GridDrawCmd> draw_grids;
};
}// namespace rbc
RBC_RTTI(rbc::GridDrawer)