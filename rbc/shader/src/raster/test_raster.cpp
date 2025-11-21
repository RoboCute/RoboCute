#include <geometry/raster.hpp>
#include <sampling/pcg.hpp>
#include <raster/raster_args.hpp>
using namespace luisa::shader;

struct v2p {
	[[POSITION]] float4 proj_pos;
	uint user_id;
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
	raster::transform_projection(o.proj_pos);
	return o;
};

[[PIXEL_SHADER]] float4 pixel(
	v2p i) {
	uint v = primitive_id();
	sampling::PCGSampler sampler(v);
	// float3 bary = barycentrics();
	auto depth = i.proj_pos.z;
	sampling::PCGSampler pixel_sample(uint2(i.proj_pos.xy) + uint2(v, i.user_id));
	// set custom depth
	// depth *= lerp(0.8f, 1.1f, pixel_sample.next());	
	// set_z_depth(depth);
	// set_z_depth_less_equal(depth);
	// set_z_depth_greater_equal(depth);
	return float4(sampler.next3f(), 1.f);
}