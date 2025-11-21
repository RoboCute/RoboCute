#pragma once
#include <luisa/std.hpp>
#include <luisa/resources/bindless_array.hpp>
#include <luisa/printer.hpp>
#include <geometry/types.hpp>
#include <sampling/pcg.hpp>
#include <virtual_tex/stream.hpp>
#include <material/openpbr_params.hpp>
#include <material/mat_codes.hpp>

#include "mats.inl"
#include "openpbr_impl.hpp"
#include "unlit_impl.hpp"
#include <std/concepts>

namespace material {
inline bool cutout(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	MatMeta meta,
	float2 uv,
	vt::VTMeta vt_meta,
	auto& rng) {
	int priority = -0x7fffffff;// TODO: make priority work
	return PolymorphicMaterial::visit(meta.mat_type, [&]<class ins>() {
		using Type = typename ins::type;
		if constexpr (requires {
						  { Type::cutout(buffer_heap, image_heap, ins::index, meta.mat_index, vt_meta, uv, priority, rng) } -> std::same_as<bool>;
					  }) {
			return Type::cutout(buffer_heap, image_heap, ins::index, meta.mat_index, vt_meta, uv, priority, rng);
		} else {
			return false;
		}
	});
}

inline float3 get_light_emission(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	uint mat_idx_buffer_heap_idx,
	uint submesh_heap_idx,
	uint mat_index,
	uint prim_id,
	float2 uv) {
	auto meta = mat_meta(buffer_heap, mat_idx_buffer_heap_idx, submesh_heap_idx, mat_index, prim_id);
	return PolymorphicMaterial::visit(meta.mat_type, [&]<class ins>() {
		using Type = typename ins::type;
		if constexpr (requires {
						  { Type::get_emission(buffer_heap, image_heap, ins::index, meta.mat_index, uv) } -> std::same_as<float3>;
					  }) {
			return Type::get_emission(buffer_heap, image_heap, ins::index, meta.mat_index, uv);
		} else {
			return float3(0);
		}
	});
}
inline bool transform_to_params(
	BindlessBuffer& buffer_heap,
	BindlessImage& image_heap,
	MatMeta meta,
	auto& params,
	uint& texture_filter,
	vt::VTMeta vt_meta,
	float2 uv,
	float4 ddxy,
	float3 input_dir,
	float3 world_pos,
	bool& reject,
	auto&&... vars) {
	return PolymorphicMaterial::visit(meta.mat_type, [&]<class ins>() {
		using Type = typename ins::type;
		if constexpr (
			requires {
				{ Type::transform_to_params(buffer_heap,
											image_heap,
											ins::index,
											meta.mat_index,
											params,
											texture_filter,
											vt_meta,
											uv,
											ddxy,
											input_dir,
											reject,
											world_pos,
											static_cast<decltype(vars)>(vars)...) } -> std::same_as<bool>;
			}) {
			return Type::transform_to_params(buffer_heap,
											 image_heap,
											 ins::index,
											 meta.mat_index,
											 params,
											 texture_filter,
											 vt_meta,
											 uv,
											 ddxy,
											 input_dir,
											 reject,
											 world_pos,
											 static_cast<decltype(vars)>(vars)...);
		} else {
			return false;
		}
	});
}
}// namespace material