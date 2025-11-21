#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<float>& buffer,
	Image<float>& img,
	uint comp) {
	auto id = dispatch_id().xy;
	auto idx = (id.x + id.y * dispatch_size().x) * comp;
	float4 value = img.read(id);
	for (uint i = 0; i < comp; ++i) {
		buffer.write(idx + i, value[i]);
	}
	return 0;
}