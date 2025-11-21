#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float> src_img,
	float3 sky_color) {
	auto id = dispatch_id().xy;
	float4 col = src_img.read(id);
	col.xyz *= sky_color;
	src_img.write(id, col);
	return 0;
}