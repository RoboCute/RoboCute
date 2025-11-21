#pragma once
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <utils/heap_indices.hpp>
namespace vt {
using namespace luisa::shader;

extern Buffer<uint>& g_vt_level_buffer;

static constexpr uint vt_chunk_size = 256u;

struct VTMeta {
	uint frame_countdown;
};
inline float4 sample_vt(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	VTMeta vt_meta,
	uint tex_heap_idx,
	float2 uv,
	float2 ddx,
	float2 ddy,
	uint& min_level,
	uint& dst_level,
	uint filter,
	uint address) {
	uint chunk_offset = max_uint32;
	if (!(any(ddx > 1.0f / float(vt_chunk_size)) && any(ddy > 1.0f / float(vt_chunk_size)))) {
		chunk_offset = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::chunk_offset_buffer_idx, tex_heap_idx);
	}
	if (chunk_offset == max_uint32) {
		min_level = 0;
		return image_heap.image_sample_grad(tex_heap_idx, uv, ddx, ddy, filter, address);
	}
	uv = fract(uv);
	auto tex_size = image_heap.image_size(tex_heap_idx);
	auto uv_diff = max(abs(ddx), abs(ddy));
	uv_diff *= float2(tex_size);
	dst_level = uint(log2(max(uv_diff.x, uv_diff.y) * 0.5f));
	auto sub_level_count = tex_size / vt_chunk_size;
	auto sub_level_coord = uint2(float2(sub_level_count) * uv);
	auto sub_level_idx = sub_level_coord.x + sub_level_coord.y * sub_level_count.x;
	uint value = vt_meta.frame_countdown << uint(4);
	value |= dst_level;
	uint buffer_offset = chunk_offset + sub_level_idx;
	g_vt_level_buffer.atomic_fetch_min(buffer_offset, value);
	min_level = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::min_level_buffer_idx, buffer_offset);
	return image_heap.image_sample_grad(tex_heap_idx, uv, ddx, ddy, min_level, filter, address);
}
inline float4 sample_vt(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	VTMeta vt_meta,
	uint tex_heap_idx,
	float2 uv,
	float target_level,
	uint& min_level,
	uint filter,
	uint address) {
	auto chunk_offset = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::chunk_offset_buffer_idx, tex_heap_idx);
	if (chunk_offset == max_uint32) {
		min_level = 0;
		return image_heap.image_sample_level(tex_heap_idx, uv, target_level, filter, address);
	}
	uv = fract(uv);
	uint2 tex_size = image_heap.image_size(tex_heap_idx);
	uint2 sub_level_count = tex_size / vt_chunk_size;
	auto sub_level_coord = uint2(float2(sub_level_count) * uv);
	auto sub_level_idx = sub_level_coord.x + sub_level_coord.y * sub_level_count.x;
	uint value = vt_meta.frame_countdown << uint(4);
	value |= uint(target_level);
	uint buffer_offset = chunk_offset + sub_level_idx;
	g_vt_level_buffer.atomic_fetch_min(buffer_offset, value);
	min_level = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::min_level_buffer_idx, buffer_offset);
	return image_heap.image_sample_level(tex_heap_idx, uv, max((float)min_level, target_level), filter, address);
}
inline float4 sample_vt_quiet(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	VTMeta vt_meta,
	uint tex_heap_idx,
	float2 uv,
	float2 ddx,
	float2 ddy,
	uint& min_level,
	uint filter,
	uint address) {
	uint chunk_offset = max_uint32;
	if (!(any(ddx > 1.0f / float(vt_chunk_size)) && any(ddy > 1.0f / float(vt_chunk_size)))) {
		chunk_offset = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::chunk_offset_buffer_idx, tex_heap_idx);
	}
	if (chunk_offset == max_uint32) {
		return image_heap.image_sample_grad(tex_heap_idx, uv, ddx, ddy, filter, address);
	}
	uv = fract(uv);
	auto tex_size = image_heap.image_size(tex_heap_idx);
	auto uv_diff = max(abs(ddx), abs(ddy));
	uv_diff *= float2(tex_size);
	auto sub_level_count = tex_size / vt_chunk_size;
	auto sub_level_coord = uint2(float2(sub_level_count) * uv);
	auto sub_level_idx = sub_level_coord.x + sub_level_coord.y * sub_level_count.x;
	uint buffer_offset = chunk_offset + sub_level_idx;
	min_level = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::min_level_buffer_idx, buffer_offset);
	return image_heap.image_sample_grad(tex_heap_idx, uv, ddx, ddy, min_level, filter, address);
}
inline float4 sample_vt_quiet(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	VTMeta vt_meta,
	uint tex_heap_idx,
	float2 uv,
	float target_level,
	uint& min_level,
	uint filter,
	uint address) {
	auto chunk_offset = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::chunk_offset_buffer_idx, tex_heap_idx);
	if (chunk_offset == max_uint32) {
		return image_heap.image_sample_level(tex_heap_idx, uv, target_level, filter, address);
	}
	uv = fract(uv);
	uint2 tex_size = image_heap.image_size(tex_heap_idx);
	uint2 sub_level_count = tex_size / vt_chunk_size;
	auto sub_level_coord = uint2(float2(sub_level_count) * uv);
	auto sub_level_idx = sub_level_coord.x + sub_level_coord.y * sub_level_count.x;
	uint buffer_offset = chunk_offset + sub_level_idx;
	min_level = buffer_heap.uniform_idx_buffer_read<uint>(heap_indices::min_level_buffer_idx, buffer_offset);
	return image_heap.image_sample_level(tex_heap_idx, uv, max((float)min_level, target_level), filter, address);
}
}// namespace vt
