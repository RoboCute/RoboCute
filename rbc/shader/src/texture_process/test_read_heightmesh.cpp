#include <luisa/std.hpp>
#include <sampling/pcg.hpp>
#include <virtual_tex/vt_bilinear.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	BindlessImage& heap,
	Image<float>& out,
	Image<uint>& offset_texture,
	Buffer<uint2>& level_buffer,
	uint max_level,
	// Global index
	uint2 sample_index,
	// 1 << max_mip_level
	uint level_size) {
	auto sample_uv = (0.5f + float2(dispatch_id().xy)) / float2(dispatch_size().xy);
	auto result = vt::manual_bilinear_sample(
		[&](int2 index, float2 uv, uint filter, uint address) -> float4 {
		if (any(index > 0)) {
			uv = ite(index > 0, float2(1.0f), uv);
			index = int2(0);
		} else if (any(index < 0)) {
			uv = ite(index < 0, float2(0.0f), uv);
			index = int2(0);
		}
		auto offset = offset_texture.read(index).x;
		auto sample_level = uint2(uv * float2(level_size));
		auto index_level = level_buffer.read(sample_level.y * level_size + sample_level.x + offset);

		auto sample_uv = fract(uv * float2(1u << (max_level - index_level.y - 1)));
		return heap.image_sample_level(index_level.x, sample_uv, 0, filter, address);
	},
		uint2(256 * 8),
		sample_index,
		sample_uv,
		0);
	uint2 log_value = (dispatch_size().xy - 1u) / uint2(1, 2);
	out.write(dispatch_id().xy, float4(result, 1.f));
	return 0;
}