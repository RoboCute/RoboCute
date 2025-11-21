#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<uint>& buffer,
	Image<float>& img) {
	auto coord = dispatch_id().xy;
	auto idx = coord.x + coord.y * dispatch_size().x;
	auto buffer_val = buffer.read(idx);
	auto value = float4((uint4(buffer_val) >> uint4(24, 16, 8, 0)) & uint4(255u)) / 255.0f;
	img.write(coord, value);
	return 0;
}