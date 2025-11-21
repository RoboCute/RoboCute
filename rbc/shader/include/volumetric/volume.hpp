#pragma once
#include <luisa/std.hpp>
#include <material/openpbr_params.hpp>
#include <luisa/printer.hpp>

// #define DEBUG

namespace mtl {

using namespace luisa::shader;

struct Volume {
	static constexpr uint32 MAX_VOLUME_STACK_SIZE = 2u;

	float3 extinction;// units of inverse length
	float3 albedo;	  // single-scattering albedo
	float anisotropy; // phase function anisotropy in [-1, 1]
	float ior;

	void fill_from_transmission(float scatter_anisotropy, float3 scatter, float3 color, float depth) {
		if (depth > 0.0f) {
			float3 mu_t = -log(max(float3(1e-3f), color)) / depth;
			float3 mu_s = scatter / depth;
			float3 mu_a = mu_t - mu_s;
			mu_a -= float3(min(0.0f, reduce_min(mu_a)));
			extinction = mu_a + mu_s;
			albedo = ite(extinction != 0.0f, mu_s / extinction, float3(0));
			anisotropy = clamp(scatter_anisotropy, -0.99f, 0.99f);
		} else {
			extinction = 0.0f;
			albedo = 0.0f;
		}
	}
	void fill_from_subsurface(float scatter_anisotropy, float3 radius, float3 color) {
		float g = clamp(scatter_anisotropy, -0.95f, 0.95f);// scattering anisotropy
		float3 A = color;
		float3 s = 4.09712f + 4.20863f * A - sqrt(9.59217f + 41.6808f * A + 17.7126f * sqr(A));
		float3 s2 = sqr(s);
		extinction = 1.0f / max(float3(3e-4f), radius);
		albedo = (1.0f - s2) / max((1.0f - g * s2), float3(1e-6f));// single-scattering albedo accounting for anisotropy
		anisotropy = g;
	}

	void fill_from_transmission(openpbr::Parameter::Transmission const& p) {
		fill_from_transmission(
			p.scatter_anisotropy,
			p.scatter,
			p.color.spectral(),
			p.depth);
	}
	void fill_from_subsurface(openpbr::Parameter::Subsurface const& p) {
		fill_from_subsurface(
			p.scatter_anisotropy,
			p.radius,
			p.color.spectral());
	}
};

}// namespace mtl
