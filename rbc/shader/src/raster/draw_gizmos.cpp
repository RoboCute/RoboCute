#include <raster/raster_args.hpp>
#include <geometry/raster.hpp>
#include <luisa/raster/attributes.hpp>
using namespace luisa::shader;
struct PixelArgs {
    uint2 clicked_pixel;
    float3 from_mapped_color;
    float3 to_mapped_color;
};
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
    Buffer<float4> &clicked_id,
    PixelArgs args) {
    auto curr_id = uint2(i.proj_pos.xy);
    if (all(curr_id == args.clicked_pixel)) {
        auto inst_id = object_id();
        float4 result;
        result.xyz = i.local_pos.xyz;
        result.w = bit_cast<float>(primitive_id());
        clicked_id.write(inst_id, result);
    }
    if (distance(args.from_mapped_color, i.color.xyz) < 1e-2f) {
        i.color.xyz = args.to_mapped_color;
    }
    i.color.w = 1.f;
    return i.color;
}