#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>

namespace mtl {

using namespace luisa::shader;

// "Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines", Zeltner et al. SIGGRAPH 2022 Talk
// https://tizianzeltner.com/projects/Zeltner2022Practical/sheen.pdf
class ZeltnerLTCSheenBRDF {
	spectrum::SpectrumColor color;
	float roughness;

	static float
	dir_albedo(float x, float y) {
		float s = y * (0.0206607f + 1.58491f * y) / (0.0379424f + y * (1.32227f + y));
		float m = y * (-0.193854f + y * (-1.14885f + y * (1.7932f - 0.95943f * y * y))) / (0.046391f + y);
		float o = y * (0.000654023f + (-0.0207818f + 0.119681f * y) * y) / (1.26264f + y * (-1.92021f + y));
		return exp(-0.5f * sqr((x - m) / s)) / (s * sqrt(2.0f * pi)) + o;
	}
	static float ltc_aInv(float x, float y) {
		return (2.58126 * x + 0.813703 * y) * y / (1.0 + 0.310327 * x * x + 2.60994 * x * y);
	}
	static float ltc_bInv(float x, float y) {
		return sqrt(1.0 - x) * (y - 1.0) * y * y * y / (0.0000254053 + 1.71228 * x - 1.71506 * x * y + 1.34174 * y * y);
	}
	// V and N are assumed to be unit floattors.
	static float3x3 orthonormal_basis_ltc(float3 wi) {
		// Generate a tangent floattor in the plane of V and N.
		// This required to correctly orient the LTC lobe.
		float3 X = float3(wi.x, wi.y, 0.0f);
		float lensqr = dot(X, X);
		if (lensqr > 0.0f) {
			X *= rsqrt(lensqr);
			return float3x3(X, cross(float3(0, 0, 1), X), float3(0, 0, 1));
		}
		// If lensqr == 0, then V == N, so any orthonormal basis will do.
		return float3x3(float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1));
	}

public:
	static constexpr BSDFFlags const flags = BSDFFlags::DiffuseReflection;
	ZeltnerLTCSheenBRDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		color = p.fuzz.color;
		roughness = clamp(p.fuzz.roughness, 0.01f, 1.0f);
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result;
		if (wi.z < 0.0f || wo.z < 0.0f || !(data.sampling_flags & BSDFFlags::DiffuseReflection)) {
			return result;
		}
		auto toLTC = transpose(orthonormal_basis_ltc(wi));
		float3 w = toLTC * wo;
		float aInv = ltc_aInv(wi.z, roughness);
		float bInv = ltc_bInv(wi.z, roughness);
		// Transform w to original configuration (clamped cosine).
		//                 |aInv    0 bInv|
		// w2 = M^-1 . w = |   0 aInv    0| . w
		//                 |   0    0    1|
		float3 w2 = float3(aInv * w.x + bInv * w.z, aInv * w.y, w.z);
		float lenSqr = dot(w2, w2);
		// D(w) = Do(M^-1.w / ||M^-1.w||) . |M^-1| / ||M^-1.w||^3
		//      = Do(M^-1.w) . |M^-1| / ||M^-1.w||^4
		//      = Do(w2) . |M^-1| / dot(w2, w2)^2
		//      = Do(w2) . aInv^2 / dot(w2, w2)^2
		//      = Do(w2) . (aInv / dot(w2, w2))^2
		auto Ri = dir_albedo(wi.z, roughness);
		result.val = color.spectral() * (Ri * max(w2.z, 0.0f) * inv_pi * sqr(aInv / lenSqr));
		result.flags = flags;
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		float2 rand = data.rand.xy;
		BSDFSample result;
		if (wi.z < 0.0f || !(data.sampling_flags & BSDFFlags::DiffuseReflection)) {
			return result;
		}
		float3 w2 = cosine_sample_hemisphere(rand);

		float aInv = ltc_aInv(wi.z, roughness);
		float bInv = ltc_bInv(wi.z, roughness);

		// Transform w2 from original configuration (clamped cosine).
		//              |1/aInv      0 -bInv/aInv|
		// w = M . w2 = |     0 1/aInv          0| . w2
		//              |     0      0          1|
		float3 w = float3(w2.x / aInv - w2.z * bInv / aInv, w2.y / aInv, w2.z);

		float lenSqr = dot(w, w);
		w *= rsqrt(lenSqr);

		auto fromLTC = orthonormal_basis_ltc(wi);
		result.wo = fromLTC * w;
		if (result.wo.z <= 0.0f) {
			result.wo = float3{};
			return result;
		}
		// D(w) = Do(w2) . ||M.w2||^3 / |M|
		//      = Do(w2 / ||M.w2||) . ||M.w2||^4 / |M|
		//      = Do(w) . ||M.w2||^4 / |M| (possible because M doesn't change z component)
		//      = Do(w) . dot(w, w)^2 * aInv^2
		//      = Do(w) . (aInv * dot(w, w))^2
		result.pdf = max(w.z, 0.0f) * inv_pi * sqr(aInv * lenSqr);

		auto Ri = dir_albedo(wi.z, roughness);
		w2 = float3(aInv * w.x + bInv * w.z, aInv * w.y, w.z);
		lenSqr = dot(w2, w2);
		result.throughput.val = color.spectral() * (Ri * max(w2.z, 0.0f) * inv_pi * sqr(aInv / lenSqr));
		result.throughput.flags = flags;
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		if (wi.z < 0.0f || wo.z <= 0.0f) return 0.0f;
		auto toLTC = transpose(orthonormal_basis_ltc(wi));
		float3 w = toLTC * wo;
		float aInv = ltc_aInv(wi.z, roughness);
		return max(w.z, 0.0f) * inv_pi * sqr(aInv * dot(w, w));
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 1.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		if (wi.z < 0) return 1.0f;
		return 1.0f - dir_albedo(wi.z, roughness);
	}
	float3 energy(float3 wi, auto& data) const {
		if (data.sampling_flags & BSDFFlags::DiffuseReflection) {
			return color.color(data.spectrumed) * dir_albedo(wi.z, roughness);
		}
		return 0.0f;
	}
};

using FuzzBRDF = ZeltnerLTCSheenBRDF;

}// namespace mtl