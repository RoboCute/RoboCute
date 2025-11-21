#pragma once

#include <luisa/std.hpp>
#include <bsdfs/base/bsdf_flags.hpp>
#include <bsdfs/base/sample.hpp>
#include <material/openpbr_params.hpp>
#include <spectrum/spectrum.hpp>
#include <bsdfs/base/specialized_fresnels.hpp>
#include <bsdfs/base/microfacet.hpp>
#include <std/concepts>
#include <volumetric/volume.hpp>

#include <std/inplace_vector>

namespace mtl {

using namespace luisa::shader;

template<class ExtraParams>
struct ClosureData {
	ShadingDetail detail = ShadingDetail::Default;
	bool spectrumed = spectrum::use_spectrum;
	bool geometry_thin_walled = false;
	bool selected_wavelength = false;
	float3 lambda;
	float ray_t;
	float inv_out_ior = 1.0f;
	float original_ior;
	uint hero_wavelength_index;
	BSDFFlags sampling_flags = BSDFFlags::All;
	float3 rand;
	GGXMicrofacet specular_microfacet;

	// float rayConeWidth;
	// float occlusion;

	std::conditional_t<
		requires {
			ExtraParams::thin_film;
		},
		SpecularFresnel,
		SpecularFresnelNoThinFilm>
		specular_fresnel;
	std::conditional_t<
		requires {
			ExtraParams::base;
		}, std::conditional_t<requires {
			ExtraParams::thin_film;
		}, ConductorFresnel, ConductorFresnelNoThinFilm>,
		bool>
		conductor_fresnel;
	std::conditional_t<
		requires {
			ExtraParams::coat;
		}, Onb,
		bool>
		coat_local_onb;

	ClosureData(auto& p, auto& ep, float3 lambda, ShadingDetail detail) : geometry_thin_walled(p.geometry.thin_walled),
																		  lambda(lambda),
																		  detail(detail) {
		original_ior = p.specular.ior;
		if (detail != ShadingDetail::Default) {
			if (detail == ShadingDetail::IndirectDiffuse) {
				p.specular.roughness = max(p.specular.roughness, 0.25f);
			}
			p.specular.roughness_anisotropy *= 0.8f;
			p.specular.roughness = lerp(p.specular.roughness, 1.0f, saturate(p.specular.roughness * 1.5f) * 0.5f);
		}
		if (geometry_thin_walled) {
			if constexpr (requires {
							  ep.transmission;
						  }) {
				ep.transmission.depth /= max(1e-10f, p.geometry.thickness);
			}
		}
		if constexpr (requires {
						  ep.coat;
					  }) {
			coat_local_onb.normal = p.geometry.onb.to_local(ep.coat.coat_onb.normal);
			coat_local_onb.tangent = p.geometry.onb.to_local(ep.coat.coat_onb.tangent);
			coat_local_onb.bitangent = cross(coat_local_onb.normal, coat_local_onb.tangent);
		}
	}
	void init(auto const& p, auto const& ep) {
		specular_fresnel.init(p, ep, lambda, inv_out_ior);
		if constexpr (requires {
						  ep.base;
					  }) {
			conductor_fresnel.init(p, ep, lambda, inv_out_ior);
		}
		auto roughness = p.specular.roughness;
		if constexpr (requires {
						  ep.coat;
					  }) {
			roughness = roughness_overlap(
				roughness,
				ep.coat.roughness,
				p.weight.coat * ep.coat.roughening);
		}
		specular_microfacet.init(specular_ndf_roughnesses(roughness, p.specular.roughness_anisotropy));
	}
};

// trait_struct BSDFBase {
// public:
// 	static constexpr BSDFFlags flags = BSDFFlags::None;
// 	Throughput eval(float3 wi, float3 wo, ClosureData & data) const;
// 	BSDFSample sample(float3 wi, ClosureData & data, auto& volume_stack) const;
// 	float pdf(float3 wi, float3 wo, ClosureData & data) const;
// 	float3 tint_out(float3 wo, ClosureData & data, float3 base_energy) const;
// 	float3 trans(float3 wi, ClosureData & data) const;
// 	float3 energy(float3 wi, ClosureData & data) const;
// };

template<class T>
concept BSDF = requires(std::remove_cvref_t<T> bsdf,
						float3 wi,
						float3 wo,
						ClosureData<openpbr::Parameter>& data,
						openpbr::Parameter& p,
						float3 base_energy,
						std::inplace_vector<mtl::Volume, 0>& volume_stack) {
	{ bsdf.init(wi, p, p, data) } -> std::same_as<void>;
	{ bsdf.eval(wi, wo, data) } -> std::same_as<Throughput>;
	{ bsdf.sample(wi, data, volume_stack) } -> std::same_as<BSDFSample>;
	{ bsdf.pdf(wi, wo, data) } -> std::same_as<float>;
	{ bsdf.tint_out(wo, data) } -> std::same_as<float3>;
	{ bsdf.trans(wi, data, base_energy) } -> std::same_as<float3>;
	{ bsdf.energy(wi, data) } -> std::same_as<float3>;
} && std::is_same_v<BSDFFlags, std::remove_cvref_t<decltype(std::remove_cvref_t<T>::flags)>>;

// static_assert(mtl::BSDF<mtl::BSDFBase>, "BSDFBase must satisfy BSDF concept");

template<class T>
concept FloatWeight = requires(std::remove_cvref_t<T> w, openpbr::Parameter& p, ClosureData<openpbr::Parameter>& data) {
	{ w.value() } -> std::same_as<float>;
	{ w.init(p, p, data) } -> std::same_as<void>;
};

}// namespace mtl