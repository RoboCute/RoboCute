#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/utils.hpp>
#include <luisa/resources/common_extern.hpp>

namespace mtl {

using namespace luisa::shader;

class GGXMicrofacet {
public:
	float2 alpha;

	constexpr GGXMicrofacet() = default;

	void init(float2 alpha) {
		this->alpha = alpha;
	}

	bool effectivelySmooth() const { return reduce_max(alpha) < 1e-4f; }

	float lambda(float3 w) const {
		return (sqrt(1.0f + reduce_sum(sqr(alpha * w.xy)) / sqr(w.z)) - sign(w.z)) * 0.5f;
	}
	float D(float3 h) const {
		float result = rcp(pi * reduce_product(alpha) * sqr(reduce_sum(sqr(h.xy / alpha)) + sqr(h.z)));
		// Prevent potential numerical issues in other stages of the model
		return result * h.z > 1e-20f ? result : 0.f;
	}
	float G1(float3 w) const {
		return 1.0f / lambda(-w);
	}
	float G2r(float3 wi, float3 wo) const {
		if (wi.z < 0.0f) {
			wi = -wi;
			wo = -wo;
		}
		return 1.0f / (lambda(-wi) + lambda(wo));
	}
	float G2t(float3 wi, float3 wo) const {
		if (wi.z < 0.0f) {
			wi = -wi;
			wo = -wo;
		}
		return beta(lambda(-wi), lambda(wo));
	}
	float3 sample(float3 wi, float2 rand, bool reflect_only = false) const {

		float3 vh = normalize(float3(alpha * wi.xy, wi.z));

		// "Sampling Visible GGX Normals with Spherical Caps"
		// Jonathan Dupuy & Anis Benyoub - High Performance Graphics 2023
		float phi = (2 * pi) * rand.x;
		float k = 1.0f;
		// If we know we will be reflecting the view vector around the sampled micronormal, we can
		// tweak the range a bit more to eliminate some of the vectors that will point below the horizon
		// "Bounded VNDF Sampling for the Smith–GGX BRDF"
		// Yusuke Tokuyoshi & Kenta Eto - Proc. ACM Comput. Graph. Interact. Tech. 2024
		if (reflect_only) {
			float a = saturate(min(alpha.x, alpha.y));
			float s = 1.0f + length(wi.xy);
			float a2 = a * a, s2 = s * s;
			k = (s2 - a2 * s2) / (s2 + a2 * wi.z * wi.z);
		}

		float Z = lerp(1.0f, -k * vh.z, rand.y);
		float sin_h = sqrt(saturate(1 - Z * Z));
		float X = sin_h * cos(phi);
		float Y = sin_h * sin(phi);
		float3 h = float3(X, Y, Z) + vh;

		// unstretch
		return normalize(float3(alpha * h.xy, max(0.0f, h.z)));
	}

	float pdf(float3 wi, float3 h, bool reflect_only = false) const {
		float LenV = length(float3(wi.x * alpha.x, wi.y * alpha.y, wi.z));
		float k = 1.0f;
		if (reflect_only) {
			float a = saturate(min(alpha.x, alpha.y));
			float s = 1.0f + length(wi.xy);
			float ka2 = a * a, s2 = s * s;
			k = (s2 - ka2 * s2) / (s2 + ka2 * wi.z * wi.z);// Eq. 5
		}
		return (2.0f * D(h) * dot(wi, h)) / (k * wi.z + LenV);
	}

	float alpha2() const {
		return 0.5 * reduce_sum(sqr(alpha));
	}

	float3 dir_albedo(float cos_theta, float3 f0, float3 f90) const {
		// Rational quadratic fit to Monte Carlo data for reflective GGX directional albedo.
		float x = cos_theta;
		float y2 = alpha2();
		float x2 = sqr(x);
		float y = sqrt(y2);
		float4 r = float4(0.1003, 0.9345, 1.0, 1.0) +
				   float4(-0.6303, -2.323, -1.765, 0.2281) * x +
				   float4(9.748, 2.229, 8.263, 15.94) * y +
				   float4(-2.038, -3.748, 11.53, -55.83) * x * y +
				   float4(29.34, 1.424, 28.96, 13.08) * x2 +
				   float4(-8.245, -0.7684, -7.507, 41.26) * y2 +
				   float4(-26.44, 1.436, -36.11, 54.9) * x2 * y +
				   float4(19.99, 0.2913, 15.86, 300.2) * x * y2 +
				   float4(-5.448, 0.6286, 33.37, -285.1) * x2 * y2;
		float2 AB = saturate(r.xy / r.zw);
		return f0 * AB.x + f90 * AB.y;
	}

	float2 dir_albedo(float cos_theta, float ior) const {
		if (cos_theta < 0.0f) {
			cos_theta = -cos_theta;
			ior = rcp(ior);
		}
		float3 size(32);
		float3 uvw = float3(cos_theta, pow(alpha2(), 0.25f), abs((1.0f - ior) / (1.0f + ior)));
		uvw = uvw * ((size - 1.0f) / size) + float3(0.5f) / size;
		auto lut_val = g_volume_heap.uniform_idx_volume_sample(heap_indices::transmission_ggx_energy_idx, uvw, Filter::LINEAR_POINT, Address::EDGE);
		auto result = ior > 1.0f ? lut_val.xy : lut_val.zw;
		return result;
	}

	float dir_albedo(float cos_theta) const {
		return dir_albedo(cos_theta, float3{1.0f}, float3{1.0f}).x;
	}
};

}// namespace mtl