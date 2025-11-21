#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<uint>& buffer,
	Image<float>& img) {
	auto id = dispatch_id().xy;
	auto idx = (id.x + id.y * dispatch_size().x);
	float4 value = img.read(id);
	value.xyz = pow(value.xyz, 1.0f / 2.2f);
	uint result;
	for (uint i = 0; i < 4; ++i) {
		result <<= 8;
		result |= (uint)(value[3 - i] * 255.99);
	}
	buffer.write(idx, result);
	return 0;
}