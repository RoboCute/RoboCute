#include <luisa/std.hpp>

using namespace luisa::shader;
[[kernel_2d(256, 1)]] int kernel_temp(
	Buffer<COPY_TYPE> dst, Buffer<COPY_TYPE> src, Buffer<uint> indices) {
	auto element_count = dispatch_size().y;
	uint2 dsp_id = dispatch_id().xy;
	uint dst_idx = indices.read(dsp_id.x) * element_count;
	uint src_idx = dsp_id.x * element_count;
	dst.write(dst_idx + dsp_id.y, src.read(src_idx + dsp_id.y));
	return 0;
}