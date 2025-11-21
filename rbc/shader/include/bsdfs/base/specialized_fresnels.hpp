#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/fresnel.hpp>
#include <bsdfs/base/utils.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

using namespace luisa::shader;

class SpecularFresnel {
	ThinFilmFresnel<DielectricFresnel> fresnel;
	float tf_weight;

public:
	float vec_ior;

	spectrum::SpectrumColor specular_color;

	SpecularFresnel() = default;

	void init(auto const& p, auto const& ep, float3 lambda, float inv_out_ior) {
		vec_ior = p.specular.ior * inv_out_ior;
		specular_color = p.specular.color;

		auto spec_ior = p.specular.ior * inv_out_ior;
		if constexpr (requires {
						  ep.coat;
					  }) {
			spec_ior = ior_adjustment(spec_ior, ep.coat.ior * inv_out_ior, p.weight.coat);
		}
		spec_ior = ior_adjustment(spec_ior, p.weight.specular);

		fresnel.base.init(spec_ior);
		auto tf_ior = ep.thin_film.ior * inv_out_ior;
		if constexpr (requires {
						  ep.coat;
					  }) {
			tf_ior = ior_adjustment(tf_ior, ep.coat.ior * inv_out_ior, p.weight.coat);
		}
		tf_ior = ior_adjustment(tf_ior, p.weight.specular);
		fresnel.init(ep.thin_film.thickness, tf_ior, lambda);
		tf_weight = p.weight.thin_film;
	}

	float ior() const {
		return vec_ior;
	}
	float energy_ior() const {
		return fresnel.base.ior;
	}
	float3 unpolarized(float cos_theta) const {
		float3 R = 0.0f;
		if (tf_weight < 1.0f) {
			R = fresnel.base.unpolarized(cos_theta);
		}
		if (tf_weight > 0.0f) {
			R *= 1.0f - tf_weight;
			R += tf_weight * fresnel.unpolarized(cos_theta);
		}
		return R * specular_color.spectral();
	}
	std::pair<float3, float3> unpolarized_rt(float cos_theta) const {
		bool tint = true;
		if (cos_theta < 0) {
			auto t0 = 1.0f - (1.0f - sqr(cos_theta)) * sqr(ior());
			if (t0 > 0) {
				cos_theta = sqrt(t0);
			} else {
				return {1.0f, 0.0f};
			}
			tint = false;
		}
		float3 R = 0.0f;
		if (tf_weight < 1.0f) {
			R = fresnel.base.unpolarized(cos_theta);
		}
		if (tf_weight > 0.0f) {
			R *= 1.0f - tf_weight;
			R += tf_weight * fresnel.unpolarized(cos_theta);
		}
		float3 T = 1.0f - R;
		if (tint) R *= specular_color.spectral();
		return {R, T};
	}
	auto refract(float3 wi, float3 n) const {
		return DielectricFresnel{ior()}.refract(wi, n);
	}
};

class ConductorFresnel {
	ThinFilmFresnel<SchlickF82tintFresnel> fresnel;
	float tf_weight;
	float3 f0_val;

public:
	spectrum::SpectrumColor f0() const {
		return spectrum::SpectrumColor(f0_val, saturate(fresnel.base.f0 * fresnel.base.weight));
	}
	float3 f90() const {
		return saturate(fresnel.base.weight);
	}

	ConductorFresnel() = default;

	void init(auto const& p, auto const& ep, float3 lambda, float inv_out_ior) {
		fresnel.init(ep.thin_film.thickness, ep.thin_film.ior * inv_out_ior, lambda);
		fresnel.base.init(
			p.weight.base <= 0.0f ? float3(0.0f) : ep.base.color.spectral() * p.weight.base,
			p.specular.color.spectral(),
			1.0f,
			p.weight.specular);
		tf_weight = p.weight.thin_film;
		f0_val = p.weight.base <= 0.0f ? float3(0.0f) :
										 saturate(ep.base.color.origin() * p.weight.base * p.weight.specular);
	}

	float3 unpolarized(float cos_theta) const {
		float3 R = 0.0f;
		if (tf_weight < 1.0f) {
			R = fresnel.base.unpolarized(cos_theta);
		}
		if (tf_weight > 0.0f) {
			R *= 1.0f - tf_weight;
			R += tf_weight * fresnel.unpolarized(cos_theta);
		}
		return R;
	}
};

class SpecularFresnelNoThinFilm {
	DielectricFresnel fresnel;

public:
	float vec_ior;

	spectrum::SpectrumColor specular_color;

	SpecularFresnelNoThinFilm() = default;

	void init(auto const& p, auto const& ep, float3 lambda, float inv_out_ior) {
		vec_ior = p.specular.ior * inv_out_ior;
		specular_color = p.specular.color;

		auto spec_ior = p.specular.ior * inv_out_ior;
		if constexpr (requires {
						  ep.coat;
					  }) {
			spec_ior = ior_adjustment(spec_ior, ep.coat.ior * inv_out_ior, p.weight.coat);
		}
		spec_ior = ior_adjustment(spec_ior, p.weight.specular);

		fresnel.init(spec_ior);
	}

	float ior() const {
		return vec_ior;
	}
	float energy_ior() const {
		return fresnel.ior;
	}
	float3 unpolarized(float cos_theta) const {
		return fresnel.unpolarized(cos_theta) * specular_color.spectral();
	}
	std::pair<float3, float3> unpolarized_rt(float cos_theta) const {
		bool tint = true;
		if (cos_theta < 0) {
			auto t0 = 1.0f - (1.0f - sqr(cos_theta)) * sqr(ior());
			if (t0 > 0) {
				cos_theta = sqrt(t0);
			} else {
				return {1.0f, 0.0f};
			}
			tint = false;
		}
		float3 R = fresnel.unpolarized(cos_theta);
		float3 T = 1.0f - R;
		if (tint) R *= specular_color.spectral();
		return {R, T};
	}
	auto refract(float3 wi, float3 n) const {
		return DielectricFresnel{ior()}.refract(wi, n);
	}
};

class ConductorFresnelNoThinFilm {
	SchlickF82tintFresnel fresnel;
	float3 f0_val;

public:
	spectrum::SpectrumColor f0() const {
		return spectrum::SpectrumColor(f0_val, saturate(fresnel.f0 * fresnel.weight));
	}
	float3 f90() const {
		return saturate(fresnel.weight);
	}

	ConductorFresnelNoThinFilm() = default;

	void init(auto const& p, auto const& ep, float3 lambda, float inv_out_ior) {
		fresnel.init(
			p.weight.base <= 0.0f ? float3(0.0f) : ep.base.color.spectral() * p.weight.base,
			p.specular.color.spectral(),
			1.0f,
			p.weight.specular);
		f0_val = p.weight.base <= 0.0f ? float3(0.0f) :
										 saturate(ep.base.color.origin() * p.weight.base * p.weight.specular);
	}

	float3 unpolarized(float cos_theta) const {
		return fresnel.unpolarized(cos_theta);
	}
};

}// namespace mtl