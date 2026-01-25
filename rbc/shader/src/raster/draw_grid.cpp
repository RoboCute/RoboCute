#include <raster/raster_args.hpp>
#include <geometry/raster.hpp>
using namespace luisa::shader;

struct v2p {
    [[POSITION]] float4 proj_pos;
    float4 world_pos;
};

[[VERTEX_SHADER]] v2p vert(
    raster::AppDataBase data,
    float4x4 vp) {
    v2p o;
    o.proj_pos = vp * float4(data.pos.xyz, 1.f);
    o.world_pos = float4(data.pos.xyz, 1.f);
    raster::transform_projection(o.proj_pos);
    return o;
}

[[PIXEL_SHADER]] float4 pixel(
    v2p i,
    float3 cam_pos,
    float4 color,
    float start_decay_dist,
    float decay_length) {
    float4 col;
    auto dist = distance(cam_pos, i.world_pos.xyz);
    auto transparency = saturate(max(dist - start_decay_dist, 0.f) / max(decay_length, 1e-4f));
    col = color;
    col.w = lerp(col.w, 0.f, transparency);
    return col;
}