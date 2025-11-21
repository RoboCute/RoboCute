#pragma once
#include <luisa/std.hpp>
#include <std/array>
#include <utils/heap_indices.hpp>

namespace mtl {

constexpr float const DENOM_TOLERANCE = 1e-10f;

using namespace luisa::shader;

inline float f0_to_ior(float F0) {
	float sqrtF0 = sqrt(clamp(F0, 0.0f, 0.999f));
	return (1.0f + sqrtF0) / (1.0f - sqrtF0);
}
inline float ior_to_f0(float ior) {
	return sqr((ior - 1.0f) / (ior + 1.0f));
}
inline float3 uniform_sample_hemisphere(float2 u) {
	float sin_u = sqrt(1.0f - u.x * u.x);
	float phi = 2.0f * pi * u.y;
	return float3(sin_u * cos(phi), sin_u * sin(phi), u.x);
}
inline float3 cosine_sample_hemisphere(float2 u) {
	auto r = sqrt(u.x);
	auto phi = 2.0f * pi * u.y;
	return float3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
}
inline float3 uniform_sample_sphere(float2 u) {
	auto phi = 2.0f * pi * u.x;
	auto cos_theta = u.y * 2.0f - 1.0f;
	auto sin_theta = sqrt(1.0f - cos_theta * cos_theta);
	return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}
inline float2 specular_ndf_roughnesses(float roughness, float anisotropy) {
	float2 alpha;
	if (anisotropy == 0.0f) {
		alpha = sqr(roughness);
		return alpha;
	}
	alpha.x = sqr(roughness) * sqrt(2.0f / (1.0f + sqr(1.0f - anisotropy)));
	alpha.y = (1.0f - anisotropy) * alpha.x;
	if (reduce_max(alpha) < 1e-4f) {
		alpha = 0.0f;
	} else {
		alpha = max(alpha, float2(1e-4f));
	}
	return alpha;
}
inline float roughness_overlap(float roughness, float coat_roughness, float weight) {
	if (weight == 0.0f) return roughness;
	float coated_roughness = pow(min(1.0f, pow4(roughness) + 2.0f * pow4(coat_roughness)), 1.0f / 4.0f);
	return lerp(roughness, coated_roughness, weight);
}
inline float ior_adjustment(float ior, float coat_ior, float coat_weight) {
	if (coat_weight == 0.0f) return ior;
	float bc = coat_ior > ior ? coat_ior / ior : ior / coat_ior;
	return lerp(ior, bc, coat_weight);
}
inline float ior_adjustment(float ior, float specular_weight) {
	float e = sign(ior - 1.0f) * sqrt(clamp(specular_weight * ior_to_f0(ior), 0.0f, 0.999f));
	return (1.0f + e) / (1.0f - e);
}
inline float3 coat_view_dependent_absorption(float3 color, float mu, float ior, float modifier = 0.0f) {
	float mu_t = sqrt(max(1e-4f, 1.0f - (1.0f - sqr(mu)) / sqr(ior)));
	return pow(color, float3(rcp(mu_t) + modifier));
}
inline float thin_dielectric_roughness_scaler2(float ior) {
	float eta = ior >= 1.0f ? ior : rcp(ior);
	return 3.7f * (eta - 1.0f) * sqr(eta - 0.5f) / (eta * eta * eta);
}
inline float dispersion_ior(float nd, float Vd, float scale, float lambda) {
	constexpr float const lambda_C = 656.3f;
	constexpr float const lambda_d = 587.6f;
	constexpr float const lambda_F = 486.1f;
	constexpr float const lambda_FC2 = 1.0f / (1.0f / (lambda_F * lambda_F) - 1.0f / (lambda_C * lambda_C));
	float B = (nd - 1.0f) * lambda_FC2 / max(1e-1f, Vd) * scale;// smaller than 1e-1f won't produce good results
	float A = nd - B / sqr(lambda_d);
	return A + B / sqr(lambda);
}
}// namespace mtl