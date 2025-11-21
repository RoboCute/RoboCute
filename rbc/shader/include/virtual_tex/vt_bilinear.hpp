#pragma once

#include <luisa/std.hpp>
#include <std/concepts>

using namespace luisa::shader;
namespace vt {
float3 manual_bilinear_sample(
	std::invocable_r<float4, int2, float2, uint, uint> auto const& sample_func,
	uint2 tex_full_size,
	int2 global_tex_index,
	float2 local_chunk_uv,
	uint sample_level) {
	auto tex_miped_size = float2(tex_full_size >> sample_level);
	auto tex_idx = tex_miped_size * local_chunk_uv;
	auto right_top_level = tex_idx >= tex_miped_size - 0.5f;
	auto left_done_level = tex_idx <= 0.5f;
	float2 corner_sample_uv = ite(tex_idx > float2(0.5f), local_chunk_uv + float2(1.f) / tex_miped_size, local_chunk_uv - float2(1.f) / tex_miped_size);

	int2 next_sample_idx;
	float2 sample_uv;
	float4 result = sample_func(global_tex_index, local_chunk_uv, Filter::POINT, Address::EDGE);

	// X + 1, Y
	next_sample_idx = global_tex_index;
	sample_uv = float2(corner_sample_uv.x, local_chunk_uv.y);
	next_sample_idx.x = ite(right_top_level.x, next_sample_idx.x + 1, next_sample_idx.x);
	next_sample_idx.x = ite(left_done_level.x, next_sample_idx.x - 1, next_sample_idx.x);
	sample_uv.x = ite(right_top_level.x, 0.5f / 16384.f, sample_uv.x);
	sample_uv.x = ite(left_done_level.x, 16383.5f / 16384.f, sample_uv.x);
	result += sample_func(next_sample_idx, sample_uv, Filter::POINT, Address::EDGE);
	uint2 log_value = (dispatch_size().xy - 1u) / uint2(1, 2);
	// X, Y + 1
	next_sample_idx = global_tex_index;
	sample_uv = float2(local_chunk_uv.x, corner_sample_uv.y);
	next_sample_idx.y = ite(right_top_level.y, next_sample_idx.y + 1, next_sample_idx.y);
	next_sample_idx.y = ite(left_done_level.y, next_sample_idx.y - 1, next_sample_idx.y);
	sample_uv.y = ite(right_top_level.y, 0.5f / 16384.f, sample_uv.y);
	sample_uv.y = ite(left_done_level.y, 16383.5f / 16384.f, sample_uv.y);
	result += sample_func(next_sample_idx, sample_uv, Filter::POINT, Address::EDGE);
	// X + 1, Y + 1
	next_sample_idx = global_tex_index;
	sample_uv = corner_sample_uv;
	next_sample_idx = ite(right_top_level, next_sample_idx + 1, next_sample_idx);
	next_sample_idx = ite(left_done_level, next_sample_idx - 1, next_sample_idx);
	sample_uv = ite(right_top_level, float2(0.5f / 16384.f), sample_uv);
	sample_uv = ite(left_done_level, float2(16383.5f / 16384.f), sample_uv);
	result += sample_func(next_sample_idx, sample_uv, Filter::POINT, Address::EDGE);
	return result.xyz / max(1e-5f, result.w);
}
}// namespace vt