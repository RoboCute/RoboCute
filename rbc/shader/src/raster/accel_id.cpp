#include <geometry/raster.hpp>
#include <sampling/pcg.hpp>
#include <raster/raster_args.hpp>
using namespace luisa::shader;

struct v2p {
    [[POSITION]] float4 proj_pos;
    uint user_id;
    uint prim_id_offset;
};

[[VERTEX_SHADER]] v2p vert(
    raster::AppDataBase data,
    Buffer<geometry::RasterElement> inst_buffer,
    raster::VertArgs args) {
    v2p o;
    auto inst_data = raster::get_instance_data(data, inst_buffer);
    float3 world_pos = (inst_data.local_to_world * float4(data.pos.xyz, 1.f)).xyz;
    o.proj_pos = args.view_proj * float4(world_pos, 1);
    o.user_id = inst_data.user_id;
    o.prim_id_offset = inst_data.prim_id_offset;
    raster::transform_projection(o.proj_pos);
    return o;
};

[[PIXEL_SHADER]] uint4 pixel(v2p i) {
    auto bary = barycentrics();
    return uint4(
        i.user_id,
        primitive_id() + i.prim_id_offset,
        bit_cast<uint>(bary.x),
        bit_cast<uint>(bary.y));
}