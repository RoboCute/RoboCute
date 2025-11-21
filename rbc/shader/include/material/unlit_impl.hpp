#pragma once
#include <luisa/std.hpp>
#include <material/mat_codes.hpp>
#include <material/openpbr_params.hpp>
#include <virtual_tex/stream.hpp>
#include "unlit.hpp"

namespace material {
inline float3 Unlit::get_emission(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	uint mat_type,
	uint mat_index,
	float2 uv) {
	auto mat = buffer_heap.uniform_idx_buffer_read<Unlit>(mat_type, mat_index);
	float3 col = float3(mat.color);
	if (mat.tex.valid()) {
		uv = uv * float2(mat.uv_scale) + float2(mat.uv_offset);
		col *= image_heap.image_sample(mat.tex, uv, Filter::LINEAR_POINT, Address::REPEAT).xyz;
	}
	return col;
}

inline bool Unlit::transform_to_params(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	uint mat_type,
	uint mat_index,
	auto& params,
	uint texture_filter,
	vt::VTMeta vt_meta,
	float2 uv,
	float4 ddxy,
	float3 input_dir,
	bool& reject,
	auto&&...) {
	if constexpr (requires { params.weight; })
		params.weight.specular = 0.0f;
	if constexpr (requires { params.geometry; })
		params.geometry.thin_walled = true;
	auto mat = buffer_heap.uniform_idx_buffer_read<Unlit>(mat_type, mat_index);
	float3 col = float3(mat.color);
	if (mat.tex.valid()) {
		uv = uv * float2(mat.uv_scale) + float2(mat.uv_offset);
		col *= image_heap.image_sample(mat.tex, uv, texture_filter, Address::REPEAT).xyz;
	}
	if constexpr (requires { params.emission; })
		params.emission.luminance = col;
	return false;
};
}// namespace material