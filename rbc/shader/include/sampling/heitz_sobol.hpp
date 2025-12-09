#pragma once
#include <luisa/std.hpp>
#include <luisa/resources/bindless_array.hpp>
#include <utils/heap_indices.hpp>
namespace sampling {
using namespace luisa::shader;
inline float sample_heitz_sobol_bindless(
	uint2 pixel,
	uint sample_index,
	uint sample_dimension,
	BindlessBuffer& heap) {
	pixel = pixel & 127u;
	sample_index = sample_index & 255u;
	auto value = heap.uniform_idx_buffer_read<uint>(heap_indices::sobol_256d_heap_idx,
													(sample_dimension & 255u) + sample_index * 256);
	value = value ^ heap.uniform_idx_buffer_read<uint>(heap_indices::sobol_scrambling_heap_idx,
													   (sample_dimension & 7) + (pixel.x + pixel.y * 128) * 8);
	return (0.5f + float(value)) / 256.f;
}
inline float sample_heitz_sobol(
	uint2 pixel,
	uint sample_index,
	uint sample_dimension,
	Buffer<uint>& sobol_256_buffer,
	Buffer<uint>& scrambling_buffer) {
	pixel = pixel & 127u;
	sample_index = sample_index & 255u;
	auto value = sobol_256_buffer.read((sample_dimension & 255u) + sample_index * 256);
	value = value ^ scrambling_buffer.read((sample_dimension & 7) + (pixel.x + pixel.y * 128) * 8);
	return (0.5f + float(value)) / 256.f;
}
inline float sample_heitz_sobol_rank_bindless(
	uint2 pixel,
	uint sample_index,
	uint sample_dimension,
	BindlessBuffer& heap) {
	pixel = pixel & 127u;
	sample_index = sample_index & 255u;
	auto offset = (pixel.x + pixel.y * 128) * 8 + (sample_dimension & 7);
	sample_index = sample_index ^ heap.uniform_idx_buffer_read<uint>(
									  heap_indices::sobol_ranking_heap_idx, offset);
	auto value = heap.uniform_idx_buffer_read<uint>(heap_indices::sobol_256d_heap_idx,
													(sample_dimension & 255u) + sample_index * 256);
	value = value ^ heap.uniform_idx_buffer_read<uint>(heap_indices::sobol_scrambling_heap_idx, offset);
	return (0.5f + float(value)) / 256.f;
}
struct HeitzSobol {
	uint2 pixel;
	uint sample_idx;
	uint dimension;
	float3 offset;
	HeitzSobol(uint2 pixel, uint sample_idx)
		: pixel(pixel),
		  sample_idx(sample_idx),
		  dimension(0) {
	}
	float next(BindlessBuffer& heap) {
		auto f = sampling::sample_heitz_sobol_rank_bindless(
					 pixel,
					 sample_idx,
					 dimension,
					 heap) +
				 offset.x;
		dimension += 1;
		return fract(f);
	}
	float2 next2f(
		BindlessBuffer& heap) {
		float2 f;
		f.x = next(heap);
		f.y = next(heap);
		return fract(f + offset.xy);
	}
	float3 next3f(BindlessBuffer& heap) {
		float3 f;
		f.x = next(heap);
		f.y = next(heap);
		f.z = next(heap);
		return fract(f + offset);
	}
};
}// namespace sampling