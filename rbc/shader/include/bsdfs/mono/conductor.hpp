#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/microfacet.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>
#include <bsdfs/base/specialized_fresnels.hpp>
#include <bsdfs/base/reflective_fresnel_microfacet.hpp>

namespace mtl {

using namespace luisa::shader;

class ConductorBRDF {
	float Ess = 1.0f;

public:
	static constexpr BSDFFlags const flags = BSDFFlags::SpecularReflection | BSDFFlags::DeltaReflection;

	ConductorBRDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		if (!data.specular_microfacet.effectivelySmooth())
			Ess = data.specular_microfacet.dir_albedo(wi.z, float3(1.0f), float3(1.0f)).x;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		auto result = ReflectiveFresnelMicrofacet::eval(wi, wo, data, data.specular_microfacet, data.conductor_fresnel);
		if (result && is_non_delta(result.flags)) {
			// "Practical multiple scattering compensation for microfacet models"
			// Emmanuel Turquin - 2019
			result.val *= 1.0f + data.conductor_fresnel.f0().spectral() * (1.0f - Ess) / Ess;
		}
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		auto result = ReflectiveFresnelMicrofacet::sample(wi, data, data.specular_microfacet, data.conductor_fresnel);
		if (result && is_non_delta(result.throughput.flags)) {
			// "Practical multiple scattering compensation for microfacet models"
			// Emmanuel Turquin - 2019
			result.throughput.val *= 1.0f + data.conductor_fresnel.f0().spectral() * (1.0f - Ess) / Ess;
		}
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		return ReflectiveFresnelMicrofacet::pdf(wi, wo, data, data.specular_microfacet, data.conductor_fresnel);
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 0.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		return 0.0f;
	}
	float3 energy(float3 wi, auto& data) const {
		if (wi.z < 0.0f)
			return 0.0f;
		if (data.specular_microfacet.effectivelySmooth()) {
			if (!(data.sampling_flags & BSDFFlags::DeltaReflection))
				return 0.0f;
		} else {
			if (!(data.sampling_flags & BSDFFlags::SpecularReflection))
				return 0.0f;
		}
		// "Practical multiple scattering compensation for microfacet models"
		// Emmanuel Turquin - 2019
		auto f0 = data.conductor_fresnel.f0().color(data.spectrumed);
		return data.specular_microfacet.dir_albedo(wi.z, f0, data.conductor_fresnel.f90()) * (1.0f + f0 * (1.0f - Ess) / Ess);
	}
};

}// namespace mtl