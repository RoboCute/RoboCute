#include <raster/raster_args.hpp>
#include <geometry/raster.hpp>
#include <luisa/raster/attributes.hpp>
using namespace luisa::shader;

struct AppData {
    [[POSITION]] float4 pos;
    [[COLOR]] float4 color;
};

struct v2p {
    [[POSITION]] float4 proj_pos;
    float4 color;
    float4 local_pos;
};
[[VERTEX_SHADER]] v2p vert(
    AppData appdata,
    float4x4 vp) {
    v2p o;
    o.proj_pos = vp * float4(appdata.pos.xyz, 1.f);
    o.color = appdata.color;
    o.local_pos = appdata.pos;
    raster::transform_projection(o.proj_pos);
    return o;
}

[[PIXEL_SHADER]] float4 pixel(
    v2p i,
    uint2 clicked_pixel,
    Buffer<float4> &clicked_id) {
    auto curr_id = uint2(i.proj_pos.xy);
    if (all(curr_id == clicked_pixel)) {
        auto inst_id = object_id();
        float4 result;
        result.xyz = i.local_pos.xyz;
        result.w = bit_cast<float>(primitive_id());
        clicked_id.write(inst_id, result);
    }
    return i.color;
}