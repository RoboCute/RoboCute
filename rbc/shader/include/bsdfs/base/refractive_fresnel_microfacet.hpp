#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

using namespace luisa::shader;

class [[ignore]] RefractiveFresnelMicrofacet {
public:
	static Throughput eval(float3 wi, float3 wo, auto& data, auto const& microfacet, auto const& fresnel) {
		Throughput result;
		if (microfacet.effectivelySmooth() || fresnel.ior() == 1.0f) {
			return result;
		}
		// Evaluate rough dielectric BSDF
		// Compute generalized half vector wm
		bool reflect = wi.z * wo.z > 0;
		float etap = 1.0f;
		if (!reflect)
			etap = wi.z > 0.0f ? fresnel.ior() : rcp(fresnel.ior());
		float3 wm = wi + etap * wo;
		float mlensqr = length_squared(wm);
		if (wo.z == 0 || wi.z == 0 || mlensqr == 0)
			return result;
		wm *= rsqrt(mlensqr);
		wm = wm.z > 0 ? wm : -wm;
		float moi = dot(wi, wm);
		float moo = dot(wo, wm);
		// Discard backfacing microfacets
		if (moi * wi.z < 0 || moo * wo.z < 0)
			return result;
		auto F = fresnel.unpolarized_rt(moi);
		auto D = microfacet.D(wm);
		if (reflect) {
			if (data.sampling_flags & BSDFFlags::SpecularReflection) {
				result.flags = BSDFFlags::SpecularReflection;
				result.val = F.first * D * microfacet.G2r(wi, wo) / (4.0f * abs(wi.z));
			}
		} else {
			if (data.sampling_flags & BSDFFlags::SpecularTransmission) {
				result.flags = BSDFFlags::SpecularTransmission;
				float denom = sqr(moo + moi / etap) * wi.z;
				result.val = F.second * D * microfacet.G2t(wi, wo) * abs(moi * moo / denom);
				// only available in radiance transport mode
				result.val /= sqr(etap);
			}
		}
		return result;
	}
	static BSDFSample sample(float3 wi, auto& data, auto const& microfacet, auto const& fresnel) {
		float3 rand = data.rand.xyz;
		BSDFSample result;
		float3 wm = float3(0, 0, 1);
		float mpdf = 1.0f;
		float D = 1.0f;

		if (!microfacet.effectivelySmooth() && fresnel.ior() != 1.0f) {
			if (!is_specular(data.sampling_flags))
				return result;
			// Sample rough dielectric microfacet
			auto sample_dir = wi.z > 0 ? wi : -wi;
			wm = microfacet.sample(sample_dir, rand.xy, false);
			mpdf = microfacet.pdf(sample_dir, wm, false);
			D = microfacet.D(wm);
		} else if (!is_delta(data.sampling_flags)) {
			return result;
		}

		float moi = dot(wm, wi);

		auto F = fresnel.unpolarized_rt(moi);
		float3 R = F.first;
		float3 T = F.second;

		// Compute probabilities pr and pt for sampling reflection and transmission
		float pr = spectrum::spectrum_to_weight(R);
		float pt = spectrum::spectrum_to_weight(T);
		if (!is_reflective(data.sampling_flags)) {
			pr = 0.0f;
		}
		if (!is_transmissive(data.sampling_flags)) {
			pt = 0.0f;
		}
		// Make sure modified fresnel can be refract
		auto refracted = fresnel.refract(-wi, wm);
		if (!refracted.valid()) {
			pt = 0.0f;
			R = 1.0f;
		}
		if (pr == 0.0f && pt == 0.0f)
			return result;

		float3 wo;
		if (rand.z < pt / (pr + pt)) {
			// Sample transmission at dielectric interface
			wo = refracted.out();
			if (wi.z * wo.z >= 0.0f)
				return result;
			result.throughput.flags = BSDFFlags::DeltaTransmission;
			result.pdf = pt / (pr + pt);
			if (!microfacet.effectivelySmooth() && fresnel.ior() != 1.0f) {
				result.throughput.flags = BSDFFlags::SpecularTransmission;
				// Compute PDF of rough dielectric transmission
				float moo = dot(wo, wm);
				float denom = sqr(moo + moi / (moi > 0.0f ? fresnel.ior() : rcp(fresnel.ior())));
				result.pdf *= mpdf * (abs(moo) / denom);
				T *= D * microfacet.G2t(wi, wo) * abs(moi * moo / (wi.z * denom));
			}
			result.throughput.val = T;
		} else {
			// Sample reflection at dielectric interface
			wo = reflect(-wi, wm);
			if (wi.z * wo.z < 0.0f)
				return result;
			result.throughput.flags = BSDFFlags::DeltaReflection;
			result.pdf = pr / (pr + pt);
			if (!microfacet.effectivelySmooth() && fresnel.ior() != 1.0f) {
				result.throughput.flags = BSDFFlags::SpecularReflection;
				// Compute PDF of rough dielectric reflection
				result.pdf *= mpdf / (4.0f * abs(moi));
				R *= D * microfacet.G2r(wi, wo) / abs(4.0f * wi.z);
			}
			result.throughput.val = R;
		}
		result.wo = wo;
		return result;
	}
	static float pdf(float3 wi, float3 wo, auto& data, auto const& microfacet, auto const& fresnel) {
		if (microfacet.effectivelySmooth() || fresnel.ior() == 1.0f || !is_specular(data.sampling_flags))
			return 0.0f;
		// Evaluate sampling PDF of rough dielectric BSDF
		// Compute generalized half vector _wm_
		bool reflect = wi.z * wo.z > 0;
		float etap = 1.0f;
		if (!reflect)
			etap = wi.z > 0.0f ? fresnel.ior() : rcp(fresnel.ior());
		float3 wm = wi + etap * wo;
		float mlensqr = length_squared(wm);
		if (wo.z == 0 || wi.z == 0 || mlensqr == 0)
			return 0.0f;
		wm *= rsqrt(mlensqr);
		wm = wm.z > 0 ? wm : -wm;
		float moi = dot(wi, wm);
		float moo = dot(wo, wm);
		// Discard backfacing microfacets
		if (moi * wi.z < 0 || moo * wo.z < 0)
			return 0.0f;
		auto F = fresnel.unpolarized_rt(moi);
		float3 R = F.first;
		float3 T = F.second;
		// Compute probabilities pr and pt for sampling reflection and transmission
		float pr = spectrum::spectrum_to_weight(R);
		float pt = spectrum::spectrum_to_weight(T);
		if (!is_reflective(data.sampling_flags)) {
			pr = 0.0f;
		}
		if (!is_transmissive(data.sampling_flags)) {
			pt = 0.0f;
		}
		if (pr == 0.0f && pt == 0.0f)
			return 0.0f;
		// Return PDF for rough dielectric
		float mpdf = microfacet.pdf(wi.z > 0 ? wi : -wi, wm);
		if (reflect) {
			// Compute PDF of rough dielectric reflection
			return mpdf / (4 * abs(moi)) * pr / (pr + pt);
		} else {
			// Compute PDF of rough dielectric transmission
			float denom = sqr(moo + moi / etap);
			return mpdf * (abs(moo) / denom) * pt / (pr + pt);
		}
	}
};

}// namespace mtl