#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
	Image<uint>& img,
	Buffer<uint>& buffer) {
	auto idx = buffer.read(dispatch_id().x);
	auto out_light_type = (idx >> uint(29));
	auto out_value = (idx >> 21u) & 255u;
	auto out_light_idx = idx & ((uint(1) << uint(21)) - uint(1));
	auto tex_id = uint2(out_light_idx, out_light_type);
	img.write(tex_id, uint4(out_value));
	return 0;
}