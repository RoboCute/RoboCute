#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/fresnel.hpp>
#include <bsdfs/base/microfacet.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>
#include <spectrum/spectrum.hpp>
#include <bsdfs/base/refractive_fresnel_microfacet.hpp>
#include <bsdfs/base/reflective_fresnel_microfacet.hpp>
#include <volumetric/volume.hpp>

namespace mtl {

using namespace luisa::shader;

class TransmissionBSDF {
	GGXMicrofacet thin_wall_microfacet;
	spectrum::SpectrumColor tint;
	Volume volume;

	std::pair<float3, float3> unpolarized_rt(float cos_theta, auto const& fresnel) const {
		auto F = fresnel.unpolarized_rt(cos_theta);
		float cos_theta_i;
		auto t0 = 1.0f - (1.0f - sqr(cos_theta)) / sqr(fresnel.ior());
		if (t0 > 0) {
			cos_theta_i = sqrt(t0);
		} else {
			return F;
		}
		float3 A = tint.spectral() * exp(-volume.extinction / cos_theta_i);
		float3 rFA12 = rcp(1.0f - sqr(F.first * A));
		// caculate the internal reflections
		return {F.first * (1.0f + sqr(F.second * A) * rFA12), sqr(F.second) * A * rFA12};
	}

public:
	static constexpr BSDFFlags const flags = (BSDFFlags::Specular | BSDFFlags::Delta);

	static float3 calculate_thin_wall_delta_transmission(float3 wi, auto const& p) {
		DielectricFresnel fresnel;
		fresnel.init(ior_adjustment(p.specular.ior, p.weight.specular));
		float3 R = fresnel.unpolarized(wi.z);
		float3 T = 1.0f - R;
		float cos_theta_i;
		auto t0 = 1.0f - (1.0f - sqr(wi.z)) / sqr(fresnel.ior);
		if (t0 > 0) {
			cos_theta_i = sqrt(t0);
		} else {
			return T;
		}
		float3 A;
		if (p.transmission.depth == 0.0f) {
			A = p.transmission.color.spectral();
		} else {
			Volume volume;
			volume.fill_from_transmission(p.transmission);
			A = exp(-volume.extinction / cos_theta_i);
		}
		float3 rFA12 = rcp(1.0f - sqr(R * A));
		return sqr(T) * A * rFA12;
	}

	TransmissionBSDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		if (p.transmission.depth == 0.0f) {
			tint = p.transmission.color;
		} else {
			tint = spectrum::SpectrumColor(1.0f, 1.0f);
		}
		volume.fill_from_transmission(p.transmission);
		if (p.transmission.dispersion_scale > 0.0f) {
			data.selected_wavelength = true;
			data.specular_fresnel.vec_ior = dispersion_ior(
				data.original_ior * data.inv_out_ior,
				p.transmission.dispersion_abbe_number,
				p.transmission.dispersion_scale,
				data.lambda[data.hero_wavelength_index]);
		}
		if (data.geometry_thin_walled) {
			thin_wall_microfacet.init(
				saturate(data.specular_microfacet.alpha *
						 thin_dielectric_roughness_scaler2(data.specular_fresnel.ior())));
		}
	}

	// placeholder
	float unpolarized(auto&&) const {
		return 1.0f;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		if (data.geometry_thin_walled) {
			bool transed = false;
			if (wo.z < 0.0f) {
				wo.z = -wo.z;
				transed = true;
				if (!is_transmissive(data.sampling_flags)) return Throughput{};
			} else {
				if (!is_reflective(data.sampling_flags)) return Throughput{};
			}
			Throughput result = ReflectiveFresnelMicrofacetAll::eval(wi, wo, data, thin_wall_microfacet, *this);
			if (result) {
				auto F = unpolarized_rt(dot(wi, normalize(wi + wo)), data.specular_fresnel);
				if (transed) {
					result.val *= F.second;
					result.flags = is_delta(result.flags) ? BSDFFlags::DeltaTransmission : BSDFFlags::SpecularTransmission;
				} else {
					result.val *= F.first;
				}
				// "Practical multiple scattering compensation for thin_wall_microfacet models"
				// Emmanuel Turquin - 2019
				if (is_non_delta(result.flags)) result.val /= thin_wall_microfacet.dir_albedo(wi.z);
			}
			return result;
		} else {
			Throughput result = RefractiveFresnelMicrofacet::eval(wi, wo, data, data.specular_microfacet, data.specular_fresnel);
			if (result) {
				if (is_transmissive(result.flags)) {
					result.val *= tint.spectral() * exp(-volume.extinction * data.ray_t);
				}
				if (is_non_delta(result.flags)) {
					// "Practical multiple scattering compensation for thin_wall_microfacet models"
					// Emmanuel Turquin - 2019
					result.val /= reduce_sum(data.specular_microfacet.dir_albedo(wi.z, data.specular_fresnel.ior()));
				}
			}
			return result;
		}
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		if (data.geometry_thin_walled) {
			BSDFSample result = ReflectiveFresnelMicrofacetAll::sample(wi, data, thin_wall_microfacet, *this);
			if (result) {
				auto F = unpolarized_rt(dot(wi, normalize(wi + result.wo)), data.specular_fresnel);
				// Compute probabilities pr and pt for sampling reflection and transmission
				float pr = spectrum::spectrum_to_weight(F.first);
				float pt = spectrum::spectrum_to_weight(F.second);
				if (!is_reflective(data.sampling_flags)) {
					pr = 0.0f;
				}
				if (!is_transmissive(data.sampling_flags)) {
					pt = 0.0f;
				}
				if (pr == 0.0f && pt == 0.0f) {
					result.throughput.flags = BSDFFlags::None;
					result.throughput.val = 0.0f;
					return result;
				}
				if (data.rand.z < pt / (pr + pt)) {
					result.throughput.val *= F.second;
					result.pdf *= pt / (pr + pt);
					result.throughput.flags = is_delta(result.throughput.flags) ? BSDFFlags::DeltaTransmission : BSDFFlags::SpecularTransmission;
					result.wo.z = -result.wo.z;// flip the direction for transmissive
				} else {
					result.throughput.val *= F.first;
					result.pdf *= pr / (pr + pt);
				}
				// "Practical multiple scattering compensation for thin_wall_microfacet models"
				// Emmanuel Turquin - 2019
				if (is_non_delta(result.throughput.flags)) result.throughput.val /= thin_wall_microfacet.dir_albedo(wi.z);
			}
			return result;
		} else {
			BSDFSample result = RefractiveFresnelMicrofacet::sample(wi, data, data.specular_microfacet, data.specular_fresnel);
			if (result) {
				if (is_transmissive(result.throughput.flags)) {
					result.throughput.val *= tint.spectral();
					if (wi.z >= 0.0f) {
						if (volume_stack.try_emplace_back()) {
							auto tmp_volume = volume;
							//TODO: fix compiler
							volume_stack.back([&](auto& back) {
								back = tmp_volume;
								back.ior = data.original_ior;
							});
						}
					} else {
						if (volume_stack.empty()) {
							result.throughput.val *= exp(-volume.extinction * data.ray_t);
						} else {
							data.ray_t = 0.0f;
							volume_stack.pop_back();
						}
					}
				} else if (wi.z <= 0.0f) {
					if (volume_stack.empty()) {
						result.throughput.val *= exp(-volume.extinction * data.ray_t);
						if (volume_stack.try_emplace_back()) {
							auto tmp_volume = volume;
							//TODO: fix compiler
							volume_stack.back([&](auto& back) {
								back = tmp_volume;
								back.ior = data.original_ior;
							});
						}
					}
				}
				if (is_non_delta(result.throughput.flags)) {// "Practical multiple scattering compensation for thin_wall_microfacet models"
					// Emmanuel Turquin - 2019
					result.throughput.val /= reduce_sum(data.specular_microfacet.dir_albedo(wi.z, data.specular_fresnel.ior()));
				}
			}
			return result;
		}
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		if (data.geometry_thin_walled) {
			auto F = unpolarized_rt(dot(wi, normalize(wi + wo)), data.specular_fresnel);
			// Compute probabilities pr and pt for sampling reflection and transmission
			float pr = spectrum::spectrum_to_weight(F.first);
			float pt = spectrum::spectrum_to_weight(F.second);
			if (!is_reflective(data.sampling_flags)) {
				pr = 0.0f;
			}
			if (!is_transmissive(data.sampling_flags)) {
				pt = 0.0f;
			}
			if (pr == 0.0f && pt == 0.0f)
				return 0.0f;

			bool transed = false;
			if (wo.z < 0.0f) {
				wo.z = -wo.z;
				transed = true;
			}
			auto mpdf = ReflectiveFresnelMicrofacetAll::pdf(wi, wo, data, thin_wall_microfacet, *this);
			if (transed) {
				return mpdf * pt / (pr + pt);
			} else {
				return mpdf * pr / (pr + pt);
			}
		} else {
			return RefractiveFresnelMicrofacet::pdf(wi, wo, data, data.specular_microfacet, data.specular_fresnel);
		}
	}
	float3 tint_out(float3 wo, auto& data) const {
		return data.geometry_thin_walled ? 0.0f : 1.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		if (data.geometry_thin_walled) return 0.0f;
		float2 E = data.specular_microfacet.dir_albedo(wi.z, data.specular_fresnel.ior());
		return E.y / reduce_sum(E);
	}
	float3 energy(float3 wi, auto& data) const {
		if (!data.specular_microfacet.effectivelySmooth() && data.specular_fresnel.ior() != 1.0f) {
			if (!is_specular(data.sampling_flags))
				return 0.0f;
		} else {
			if (!is_delta(data.sampling_flags))
				return 0.0f;
		}
		if (data.geometry_thin_walled) wi.z = abs(wi.z);
		float2 E = data.specular_microfacet.dir_albedo(wi.z, data.specular_fresnel.ior());
		float Er = E.x / reduce_sum(E);
		if (!is_reflective(data.sampling_flags)) {
			Er = 0.0f;
		}
		float Et = 0.0f;
		if (is_transmissive(data.sampling_flags)) {
			Et = 1.0f - Er;
		}
		return Er * data.specular_fresnel.specular_color.color(data.spectrumed) + Et * tint.color(data.spectrumed);
	}
};

}// namespace mtl