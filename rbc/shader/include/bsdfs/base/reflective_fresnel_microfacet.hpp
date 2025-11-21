#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

using namespace luisa::shader;

template<BSDFFlags CheckFlags>
class [[ignore]] ReflectiveFresnelMicrofacetImpl {
public:
	static Throughput eval(float3 wi, float3 wo, auto& data, auto const& microfacet, auto const& fresnel) {
		Throughput result;
		if (microfacet.effectivelySmooth() || wi.z < 0.0f || wo.z <= 0.0f || !(data.sampling_flags & BSDFFlags(CheckFlags & BSDFFlags::Specular))) {
			return result;
		}
		// Evaluate rough reflective dielectric BSDF
		// Compute half vector wm
		float3 wm = wi + wo;
		float mlensqr = length_squared(wm);
		if (mlensqr == 0)
			return result;
		wm *= rsqrt(mlensqr);
		float moi = dot(wi, wm);
		auto F = fresnel.unpolarized(moi);
		auto D = microfacet.D(wm);
		result.flags = BSDFFlags::SpecularReflection;
		result.val = F * (D * microfacet.G2r(wi, wo) / (4.0f * abs(wi.z)));
		return result;
	}
	static BSDFSample sample(float3 wi, auto& data, auto const& microfacet, auto const& fresnel) {
		float2 rand = data.rand.xy;
		BSDFSample result;
		if (wi.z < 0.0f)
			return result;
		float3 wm = float3(0, 0, 1);
		float mpdf = 1.0f;
		float D = 1.0f;
		if (!microfacet.effectivelySmooth()) {
			if (!(data.sampling_flags & BSDFFlags(CheckFlags & BSDFFlags::Specular)))
				return result;
			// Sample rough conductor microfacet
			wm = microfacet.sample(wi, rand, true);
			mpdf = microfacet.pdf(wi, wm, true);
			D = microfacet.D(wm);
		} else {
			if (!(data.sampling_flags & BSDFFlags(CheckFlags & BSDFFlags::Delta)))
				return result;
		}
		float3 wo = reflect(-wi, wm);
		if (wo.z <= 0.0f) {
			return result;
		}
		float moi = dot(wm, wi);
		auto F = fresnel.unpolarized(moi);

		if (microfacet.effectivelySmooth()) {
			result.throughput.flags = BSDFFlags::DeltaReflection;
			result.pdf = 1.0f;
		} else {
			result.throughput.flags = BSDFFlags::SpecularReflection;
			// Compute PDF of rough conductor reflection
			result.pdf = mpdf / (4.0f * abs(moi));
			F *= D * microfacet.G2r(wi, wo) / abs(4.0f * wi.z);
		}
		result.wo = wo;
		result.throughput.val = F;
		return result;
	}
	static float pdf(float3 wi, float3 wo, auto& data, auto const& microfacet, auto const& fresnel) {
		if (microfacet.effectivelySmooth() || wi.z < 0.0f || wo.z <= 0.0f || !(data.sampling_flags & BSDFFlags(CheckFlags & BSDFFlags::Specular)))
			return 0.0f;
		// Evaluate sampling PDF of rough conductor BSDF
		// Compute half vector wm
		float3 wm = wi + wo;
		float mlensqr = length_squared(wm);
		if (mlensqr == 0)
			return 0.0f;
		wm *= rsqrt(mlensqr);
		float moi = dot(wi, wm);
		// Compute PDF of rough conductor reflection
		return microfacet.pdf(wi, wm, true) / (4.0f * abs(moi));
	}
};

using ReflectiveFresnelMicrofacet = ReflectiveFresnelMicrofacetImpl<BSDFFlags::Reflection>;
using ReflectiveFresnelMicrofacetAll = ReflectiveFresnelMicrofacetImpl<BSDFFlags::All>;

}// namespace mtl