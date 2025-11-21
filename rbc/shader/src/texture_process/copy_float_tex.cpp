#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<float>& buffer,
	Image<float>& img) {
	auto coord = dispatch_id().xy;
	auto idx = coord.x + coord.y * dispatch_size().x;
	float4 buffer_val = float4((float)buffer.read(idx * 4), (float)buffer.read(idx * 4 + 1), (float)buffer.read(idx * 4 + 2), (float)buffer.read(idx * 4 + 3));
	img.write(coord, buffer_val);
	return 0;
}