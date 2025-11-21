#include <luisa/std.hpp>
#include <luisa/resources.hpp>

namespace luisa::shader {
[[kernel_2d(16, 8)]] int kernel(
	Image<float>& dst,
	SampleImage& src,
	bool reverse_rgb) {

	// get id & uv
	auto id = dispatch_id().xy;
	float2 uv = (float2(id) + 0.5f) / float2(dispatch_size().xy);

	// sample color
	auto dst_color = dst.read(id);
	auto src_color = src.sample(uv, Filter::LINEAR_POINT, Address::EDGE);

	// reverse RGB
	src_color = reverse_rgb ? src_color.zyxw : src_color.xyzw;

	// do alpha blend
	dst_color.xyz = dst_color.xyz * (1.0f - src_color.w) + src_color.xyz * src_color.w;

	// write color
	dst.write(id, dst_color);

	return 0;
}
}// namespace luisa::shader