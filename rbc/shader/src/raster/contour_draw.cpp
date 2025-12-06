#include <raster/raster_args.hpp>
#include <geometry/raster.hpp>
#include <luisa/std.hpp>
#include <luisa/raster/attributes.hpp>
using namespace luisa::shader;

struct v2p {
    [[POSITION]] float4 proj_pos;
};
[[VERTEX_SHADER]] v2p vert(
    raster::AppDataBase data,
    geometry::RasterElement inst_elem,
    float4x4 view_proj) {
    v2p o;
    auto inst_data = raster::get_instance_data(data, inst_elem);
    float3 world_pos = (inst_data.local_to_world * float4(data.pos.xyz, 1.f)).xyz;
    o.proj_pos = view_proj * float4(world_pos, 1);
    raster::transform_projection(o.proj_pos);
    return o;
}

[[PIXEL_SHADER]] float4 pixel(v2p i) {
    return float4(1.0);
}