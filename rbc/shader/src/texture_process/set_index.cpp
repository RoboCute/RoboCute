#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
	Image<uint>& img,
	Buffer<uint2>& indices) {
	auto id = indices.read(dispatch_id().x);
	img.write(uint2(id.x >> 16u, id.x & 65535u), uint4(id.y));
	return 0;
}