#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<float3>& vert_buffer,
	SampleImage& height_img,
	uint2 start_coord,
	float2 height_range,
	float2 hori_range) {
	auto id = dispatch_id().xy;
	float2 sample_uv = float2(start_coord + id.xy) / float2(height_img.size());
	float height = height_img.sample(sample_uv, Filter::LINEAR_POINT, Address::EDGE).x;
	height = lerp(height_range.x, height_range.y, height);
	float2 xz_pos = lerp(-hori_range.xy, hori_range.xy, float2(id.xy) / float2(dispatch_size().xy - 1u));
	vert_buffer.write(id.x + dispatch_size().x * id.y, float3(xz_pos.x, height, xz_pos.y));
	return 0;
}