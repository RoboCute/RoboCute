#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_3d(8, 4, 4)]] int kernel(
	Volume<float>& dst,
	Buffer<float>& src) {
	auto id = dispatch_id().xyz;
	auto size = dispatch_size().xyz;
	auto volume_size = size.z;
	auto lut_idx = id.x / volume_size;
	auto local_id_x = id.x % volume_size;

	auto buffer_idx = local_id_x + id.y * volume_size + id.z * volume_size * volume_size;
	buffer_idx += volume_size * volume_size * volume_size * lut_idx;
	buffer_idx *= 3;
	dst.write(
		id,
		float4(
			src.read(buffer_idx),
			src.read(buffer_idx + 1),
			src.read(buffer_idx + 2),
			1.0f));
	return 0;
}