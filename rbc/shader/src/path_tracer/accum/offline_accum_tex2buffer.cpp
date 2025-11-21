#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float> in, Buffer<float> out) {
	auto coord = dispatch_id().xy;
	auto v = in.read(coord);
	auto buffer_id = (coord.x + coord.y * dispatch_size().x) * 3;
	out.write(buffer_id, v.x);
	out.write(buffer_id + 1, v.y);
	out.write(buffer_id + 2, v.z);
	return 0;
}