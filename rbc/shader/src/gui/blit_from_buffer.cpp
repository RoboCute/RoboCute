#include <luisa/std.hpp>
#include <luisa/resources.hpp>

namespace luisa::shader {
[[kernel_2d(16, 8)]] int kernel(
	Image<float>& dst,
	Buffer<float>& src,
	uint buffer_channel) {

	// get id & uv
	auto id = dispatch_id().xy;
	float2 uv = (float2(id) + 0.5f) / float2(dispatch_size().xy);

	// sample color
	auto buffer_id = (id.x + id.y * dispatch_size().x) * buffer_channel;
	float4 src_color;
	for(int i = 0; i < buffer_channel; ++i) {
		src_color[i] = src.read(buffer_id + i);
	}
	// write color
	
	dst.write(id, src_color);

	return 0;
}
}// namespace luisa::shader