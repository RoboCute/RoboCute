#pragma once
#include <luisa/std.hpp>
namespace sampling {
using namespace luisa::shader;
inline float3 cosine_sample_hemisphere(float2 u) {
	auto r = sqrt(u.x);
	auto phi = 2.0f * pi * u.y;
	return float3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
}
inline float sd_box(float3 p, float3 b) {
	float3 d = abs(p) - b;
	return min(max(d.x, max(d.y, d.z)), 0.0f) + length(max(d, float3(0.0f))) - 0.1f;
}
inline float2 sample_uniform_disk_concentric(float2 u_in) {
	auto u = u_in * 2.0f - 1.0f;
	auto p = abs(u.x) > abs(u.y);
	auto r = ite(p, u.x, u.y);
	auto theta = ite(p, pi_over_four * (u.y / u.x), pi_over_two - pi_over_four * (u.x / u.y));
	return r * float2(cos(theta), sin(theta));
}
inline float2 sample_uniform_disk(float2 u_in) {
	float theta = 2.f * pi * u_in.x;
	float radius = sqrt(u_in.y);
	return radius * float2(cos(theta), sin(theta));
}
inline float3 uniform_sample_hemisphere(float2 u) {
	auto r = u.x;
	auto phi = 2.0f * pi * u.y;
	return float3(r * cos(phi), r * sin(phi), sqrt(1.0f - r * r));
}
inline float3 uniform_sample_sphere(float2 u) {
	auto phi = 2 * pi * u.x;
	auto cos_theta = u.y * 2.0f - 1.0f;
	auto sin_theta = sqrt(1.f - cos_theta * cos_theta);
	return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}
inline float3 uniform_sample_cone(float2 e, float cos_theta_max) {
	auto Phi = 2 * pi * e.x;
	auto CosTheta = lerp(cos_theta_max, 1.f, e.y);
	auto SinTheta = sqrt(1 - CosTheta * CosTheta);
	auto L = float3(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);
	return L;
}
inline float uniform_sample_cone_pdf(float cos_theta_max) {
	return 1.0f / (2 * pi * (1 - cos_theta_max));
}
template<concepts::float_family T>
inline T balanced_heuristic(T pdf_a, T pdf_b) {
	return pdf_a / max(pdf_a + pdf_b, T(1e-4f));
}
template<concepts::float_family T>
inline T weighted_balanced_heuristic(float mis_weight, T pdf_a, T pdf_b) {
	return ite(mis_weight < 0, 1.0f, saturate(pdf_a / max(pdf_a + pdf_b, T(1e-4f)) * mis_weight));
}

inline float3 offset_ray_origin(float3 p, float3 n) {
	float origin = 1.0f / 32.0f;
	float float_scale = 1.0f / 65536.0f;
	float int_scale = 256.0f;
	auto of_i = int3(int_scale * n);
	auto int_p = bit_cast<int3>(p);
	auto p_i_tmp = int_p + select(of_i, -of_i, p < 0.0f);
	auto p_i = bit_cast<float3>(p_i_tmp);
	return select(p_i, p + float_scale * n, abs(p) < origin);
}
constexpr inline float const offset_ray_t_min = 1e-6f;

inline float ggx_pdf_no_pi(float Roughness, float3 V, float3 dir) {
	float a = Roughness * Roughness;
	float a2 = a * a;
	float NoV = V.z;
	float NoH = dir.z;
	float VoH = dot(V, dir);
	float G_SmithV = 2 * NoV / (NoV + sqrt(NoV * (NoV - NoV * a2) + a2));
	float d = (NoH * a2 - NoH) * NoH + 1;
	float D = a2 / (pi * d * d);
	float PDF = G_SmithV * VoH * D / NoV;
	return max(PDF, 0.f);
}
inline float ggx_pdf_inv_pi(float Roughness, float3 V, float3 dir) {
	return ggx_pdf_no_pi(Roughness, V, dir) * inv_pi;
}

inline uint pack_unorm16(float v) {
	return (uint)trunc(v * 65535.f + 0.5f);
}
inline uint pack_unorm8(float v) {
	return (uint)trunc(v * 255.f + 0.5f);
}
inline float unpack_unorm16(uint packed) {
	return float(packed & 0xffff) * (1.f / 65535.f);
}
inline float unpack_unorm8(uint packed) {
	return float(packed & 0xff) * (1.f / 255.f);
}
inline uint pack_unorm_2x16(float2 v) {
	return (pack_unorm16(v.y) << 16) | pack_unorm16(v.x);
}
inline uint pack_unorm_4x8(float4 v) {
	return (pack_unorm16(v.w) << 24) | (pack_unorm16(v.z) << 16) | (pack_unorm16(v.y) << 8) | pack_unorm16(v.x);
}
inline float2 unpack_unorm_2x16(uint packed) {
	return float2(packed & 0xffff, packed >> 16) * (1.f / 65535);
}
inline float4 unpack_unorm_4x8(uint packed) {
	return float4(packed & 0xff, (packed >> 8) & 0xff, (packed >> 16) & 0xff, (packed >> 24)) * (1.f / 255.f);
}
inline float3 sphere_uv_to_direction(float3x3 world_2_sky, float2 uv) {
	auto phi = 2.f * pi * uv.x;
	auto theta = pi * uv.y;
	auto y = cos(theta);
	auto sin_theta = sin(theta);
	auto x = sin(phi) * sin_theta;
	auto z = cos(phi) * sin_theta;
	return transpose(world_2_sky) * float3(x, y, z);
}
inline float2 sphere_direction_to_uv(float3x3 world_2_sky, float3 w) {
	w = world_2_sky * w;
	auto theta = acos(w.y);
	auto phi = atan2(w.x, w.z);
	auto u = 0.5 * inv_pi * phi;
	auto v = theta * inv_pi;
	return fract(float2(u, v));
}

inline void sphere_uv_to_direction_theta(float3x3 world_2_sky, float2 uv, float& theta, float& phi, float3& dir) noexcept {
	phi = 2.f * pi * uv.x;
	theta = pi * uv.y;
	auto y = cos(theta);
	auto sin_theta = sin(theta);
	auto x = sin(phi) * sin_theta;
	auto z = cos(phi) * sin_theta;
	dir = transpose(world_2_sky) * float3(x, y, z);
}
namespace detail {
template<concepts::float_vec_family T>
T sectorize(T value) {
	return step(T(0.0), value) * 2.0f - 1.0f;
}
}// namespace detail

inline float2 encode_unit_vector(float3 v) {
	v /= dot(abs(v), float3(1));

	float2 octWrap = (float2(1.0f) - abs(v.yx)) * (ite(v.xy >= 0.0f, float2(1.f), float2(0.f)) * 2.0f - 1.0f);
	v.xy = ite(v.z >= 0.0f, v.xy, octWrap);

	return v.xy * 0.5f + 0.5f;
}

inline float3 decode_unit_vector(float2 p) {
	p = p * 2.0f - 1.0f;

	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3(p.xy, 1.0 - abs(p.x) - abs(p.y));
	float t = saturate(-n.z);
	n.xy -= t * (step(float2(0.0f), n.xy) * 2.0f - 1.0f);

	return normalize(n);
}

inline float2 octahedral_direction_to_uv(float3 normal) {
	auto aNorm = abs(normal);
	auto sNorm = detail::sectorize(normal);

	auto dir = aNorm.xz;
	auto orient = atan2(dir.x, max(dir.y, 0.0000000000000001f)) / (pi * 0.5);

	dir = float2(aNorm.y, length(aNorm.xz));
	auto pitch = atan2(dir.y, dir.x) / (pi * 0.5f);
	auto uv = float2(sNorm.x * orient, sNorm.z * (1.0f - orient)) * pitch;

	if (normal.y < 0.0f) {
		uv = sNorm.xz - abs(uv.yx) * sNorm.xz;
	}
	return uv * 0.5f + 0.5f;
}
inline float3 octahedral_uv_to_direction(float2 uv) {
	uv = uv * 2.0f - 1.0f;
	auto suv = detail::sectorize(uv);
	auto pitch = dot(float2(1), abs(uv)) * pi * 0.5f;

	if (dot(float2(1), abs(uv)) > 1.0f) {
		uv = (float2(1.0f) - abs(uv.yx)) * suv;
	}

	auto orient = (abs(uv.x) / dot(float2(1), abs(uv))) * pi * 0.5f;
	auto sOrient = sin(orient);
	auto cOrient = cos(orient);
	auto sPitch = sin(pitch);
	auto cPitch = cos(pitch);

	return float3(
		sOrient * suv.x * sPitch,
		cPitch,
		cOrient * suv.y * sPitch);
}
inline uint reverse_bits_32(uint bits) {
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	return bits;
}
inline float2 hammersley(uint index, uint num) {
	auto E1 = fract((float(index) + 0.5f) / num);
	auto E2 = float(reverse_bits_32(index)) * 2.3283064365386963e-10f;
	return float2(E1, E2);
}
inline float3 uniform_sample_disk(float2 e) {
	auto theta = 2 * pi * e.x;
	auto radius = e.y;
	return float3(radius * float2(cos(theta), sin(theta)), radius);
}
inline float3 uniform_sample_disk_concentric(float2 e) {
	auto p = 2.f * e - 1.f;
	float radius;
	float phi;
	if (abs(p.x) > abs(p.y)) {
		radius = p.x;
		phi = (pi / 4.f) * (p.y / p.x);
	} else {
		radius = p.y;
		phi = (pi / 2.f) - (pi / 4) * (p.x / p.y);
	}
	return float3(radius * cos(phi), radius * sin(phi), radius);
}
inline uint encode_atom_minmax_float(float v) {
	auto i = bit_cast<uint>(v);
	auto sign = i >> 31u;
	i = i ^ (((1u << 31u) - 1u) * sign);
	return i ^ (1u << 31u);
}
inline float decode_atom_minmax_float(uint i) {
	i = i ^ (1u << 31u);
	auto sign = (i >> 31u);
	i = i ^ (((1u << 31u) - 1u) * sign);
	return bit_cast<float>(i);
}
inline float2 ray_tri_bary(
	float3 ray_origin,
	float3 ray_dir,
	float3 a,
	float3 b,
	float3 c) {

	auto d = ray_dir;
	auto t = ray_origin - a;
	auto e1 = b - a;
	auto e2 = c - a;
	auto p = cross(d, e2);
	auto q = cross(t, e1);
	auto det = dot(p, e1);

	return float2(dot(p, t), dot(q, d)) / det;
}
inline float3 view_to_world(
	float4x4 inv_p,
	float4x4 inv_v,
	float2 screen_uv,
	float view_depth) {
	auto proj_pos = float4((screen_uv * 2.f - 1.0f), 0.5f, 1);
	auto view_pos = inv_p * proj_pos;
	view_pos /= view_pos.w;
	auto view_dir = normalize(view_pos.xyz);
	float3 dst_view_pos = view_dir * abs(view_depth / view_dir.z);
	float4 dst_world_pos = inv_v * float4(dst_view_pos, 1.f);
	return dst_world_pos.xyz / dst_world_pos.w;
}
}// namespace sampling