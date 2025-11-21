#pragma once
#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
#include <luisa/resources/common_extern.hpp>

#include <std/concepts>

using namespace luisa::shader;
struct HDRISample {
	float3 wi;
	float3 L;
	float pdf;
};
struct AliasEntry {
	float prob;
	uint alias;
};
struct AliasSample {
	uint index;
	float uu;
};
inline auto sample_alias_table(
	uint buffer_idx,
	uint n, float u_in, uint offset) noexcept {
	auto u = u_in * (float)n;
	auto i = clamp((uint)u, 0u, n - 1u);
	auto u_remapped = fract(u);
	auto entry = g_buffer_heap.uniform_idx_buffer_read<AliasEntry>(buffer_idx, i + offset);
	auto index = ite(u_remapped < entry.prob, i, entry.alias);
	auto uu = ite(u_remapped < entry.prob, u_remapped / entry.prob,
				  (u_remapped - entry.prob) / (1.0f - entry.prob));
	AliasSample r;
	r.index = index;
	r.uu = uu;
	return r;
}
inline float _directional_pdf(float p, float theta) {
	auto s = sin(theta);
	auto inv_s = ite(s > 0.f, 1.f / s, 0.f);
	return p * inv_s * (.5f * inv_pi * inv_pi);
}
inline void sample_hdri_alias_table(
	uint sky_idx,
	uint alias_entry_buffer_idx,
	float2 rand,
	float2& uv,
	uint& buffer_sample_id) {
	auto tex_size = g_image_heap.uniform_idx_image_size(sky_idx);
	auto y_sample = sample_alias_table(alias_entry_buffer_idx, tex_size.y, rand.y, 0);
	auto offset = tex_size.y + y_sample.index * tex_size.x;
	auto x_sample = sample_alias_table(alias_entry_buffer_idx, tex_size.x, rand.x, offset);
	uv = float2((float)(x_sample.index) + x_sample.uu, (float)(y_sample.index) + y_sample.uu) / float2(tex_size);
	buffer_sample_id = y_sample.index * tex_size.x + x_sample.index;
}
inline HDRISample sample_hdri(
	float3x3 world_2_sky_mat,
	uint sky_idx,
	uint alias_entry_buffer_idx,
	uint pdf_buffer_idx,
	float2 u) {
	auto tex_size = g_image_heap.uniform_idx_image_size(sky_idx);
	auto y_sample = sample_alias_table(alias_entry_buffer_idx, tex_size.y, u.y, 0);
	auto offset = tex_size.y + y_sample.index * tex_size.x;
	auto x_sample = sample_alias_table(alias_entry_buffer_idx, tex_size.x, u.x, offset);
	auto uv = float2((float)(x_sample.index) + x_sample.uu, (float)(y_sample.index) + y_sample.uu) / float2(tex_size);
	auto index = y_sample.index * tex_size.x + x_sample.index;
	auto p = g_buffer_heap.uniform_idx_buffer_read<float>(pdf_buffer_idx, index);
	float theta;
	float phi;
	float3 wi;
	sampling::sphere_uv_to_direction_theta(world_2_sky_mat, uv, theta, phi, wi);
	HDRISample s;
	if (p < 1e-4) {
		s.L = float3(0);
		s.pdf = 0.f;
	} else {
		s.L = g_image_heap.uniform_idx_image_sample(sky_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
		s.pdf = _directional_pdf(p, theta);
	}
	s.wi = wi;
	return s;
}