#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
    Buffer<float4> &result_poses,
    float3 grid_tangent,
    float3 grid_bitangent,
    float3 center,
    float2 interval_size,
    uint2 grid_size) {
    auto id = dispatch_id().x;
    auto out_id = id;
    float3 dir;
    float3 layout_dir;
    float2 line_size;
    float rate;
    // draw horizon
    line_size = (float2(grid_size) - 1.f) * interval_size;
    if (id <= grid_size.x) {
        rate = (float)id / (float)grid_size.x;
        layout_dir = grid_bitangent;
        dir = grid_tangent;
    }
    // draw vertical
    else {
        id -= (grid_size.x + 1);
        rate = (float)id / (float)grid_size.y;
        layout_dir = grid_tangent;
        dir = grid_bitangent;
        line_size = line_size.yx;
    }
    rate = saturate(rate) - 0.5f;
    auto line_pos = center + rate * layout_dir * line_size.y;
    auto start_pos = line_pos + dir * 0.5f * line_size.x;
    auto end_pos = line_pos - dir * 0.5f * line_size.x;

    result_poses.write(out_id * 2, float4(start_pos, 1.f));
    result_poses.write(out_id * 2 + 1, float4(end_pos, 1.f));
    return 0;
}