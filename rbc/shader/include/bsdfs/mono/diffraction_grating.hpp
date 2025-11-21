#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>

namespace mtl {

using namespace luisa::shader;

constexpr uint const MAX_DIFFRACTION_LOBES = 7;

// Towards Practical Physical-Optics Rendering, SIGGRAPH 2022
// SHLOMI STEINBERG, PRADEEP SEN, and LING-QI YAN
// https://dl.acm.org/doi/10.1145/3528223.3530119

class DiffractionGratingBRDF {
	spectrum::SpectrumColor color;
	float2 inv_pitch;
	float height;
	uint lobe_count;
	DiffractionGratingType type;
	float2 angle_cs;

	float alpha(float cos_theta, float lambda) const {
		const float a = sqr(cos_theta * height / lambda * (2.0f * pi));
		return exp(-a);
	}

	float lobe_intensity(int2 lobe, float cos_theta, float lambda) const {
		const float a = (4.0f * pi) * height / (lambda * cos_theta);
		float ix, iy;
		switch (type) {
			case DiffractionGratingType::Sinusoidal: {
				// Sinusoidal phase grating, with cyl_bessel_j diffraction efficiency
				ix = lobe.x == 0 ? 1 : cyl_bessel_j(lobe.x, a);
				iy = lobe.y == 0 ? 1 : cyl_bessel_j(lobe.y, a);
			} break;
			case DiffractionGratingType::Rectangular: {
				// Rectangular phase grating, with sinc diffraction efficiency
				ix = lobe.x == 0 ? 1 : sin(a / 2.0f) * sinc(pi * lobe.x / 2.0f);
				iy = lobe.y == 0 ? 1 : sin(a / 2.0f) * sinc(pi * lobe.y / 2.0f);
			} break;
			default: {
				// Linearly decreasing intensity (roughly corresponds to a from of a triangular phase grating)
				ix = lobe.x == 0 ? 1 : 1.0f / sqrt((float)abs(lobe.x));
				iy = lobe.y == 0 ? 1 : 1.0f / sqrt((float)abs(lobe.y));
			} break;
		}
		auto result = ix * iy;
		if (any(inv_pitch == float2(0.0f))) {
			result = sqr(result);
		}
		return result;
	}

	int2 sample_lobe(float cos_theta, float2& pdf, float lambda, float2 rand) const {
		// Compute the intensity of each lobe
		std::array<float, MAX_DIFFRACTION_LOBES> intensity;
		float total = 0.0f;
		for (int l = 0; l < lobe_count; ++l) {// positive lobes
			const float li = lobe_intensity(int2(l + 1, 0), cos_theta, lambda);
			total += li;
			intensity[l] = li;
		}
		// Choose a lobe
		rand = (rand - 0.5f) * 2.0f;
		float cdf = 0.0f;
		int2 lobe = 0;
		for (int l = 0; l < lobe_count; ++l) {
			const float p = intensity[l] / total;
			cdf += p;
			const bool2 select = abs(rand) < float2(cdf) && lobe == 0;
			pdf = ite(select, float2(p), pdf);
			lobe = ite(select, int2(l + 1), lobe);
		}
		pdf /= 2.0f;

		lobe = ite(rand < 0.0f, -lobe, lobe);
		lobe = ite(inv_pitch == 0.0f, int2(0), lobe);
		pdf = ite(inv_pitch == 0.0f, float2(1.0f), pdf);
		return lobe;
	}

	float3 diffract(float3 wi, int2 lobe, float lambda) const {
		const float2 p = sqrt(sqr(wi.xy) + sqr(wi.zz));
		const float2 sini = ite(p > 1e-6f, wi.xy / p, float2(0.0f));
		const float2 sino = 1e-3f * lambda * float2(lobe) * inv_pitch - sini;
		const float a = sino.x;
		const float b = sino.y;
		const float m = (sqr(a) - 1.0f) / (sqr(a * b) - 1.0f);
		const float q = 1 - sqr(b) * m;
		return normalize(float3(a * sqrt(max(0.0f, q)),
								b * sqrt(m),
								sqrt(max(0.0f, 1.0f - sqr(a) * q - sqr(b) * m))));
	}

	float dir_albedo;
	float3 h;

public:
	static constexpr BSDFFlags const flags = BSDFFlags::DeltaReflection;

	DiffractionGratingBRDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		color = p.diffraction.color;
		inv_pitch = p.diffraction.inv_pitch;
		height = 1e3f * p.diffraction.thickness;
		lobe_count = min(p.diffraction.lobe_count, MAX_DIFFRACTION_LOBES);
		type = p.diffraction.type;
		angle_cs = float2(cos(p.diffraction.angle), sin(p.diffraction.angle));

		h = float3(0, 0, 1);
		if (!data.specular_microfacet.effectivelySmooth()) {
			wi.xy = float2(angle_cs.x * wi.x - angle_cs.y * wi.y,
						   angle_cs.y * wi.x + angle_cs.x * wi.y);
			h = data.specular_microfacet.sample(wi, data.rand.xy, false);
		}

		dir_albedo = 1.0f - alpha(abs(dot(h, wi)), data.lambda[data.hero_wavelength_index]);
		data.selected_wavelength = true;
	}
	Throughput eval(float3 wi, float3 wo, auto& data) const {
		return Throughput{};
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		float2 rand = data.rand.xy;
		BSDFSample result;
		if (wi.z < 0.0f || !(data.sampling_flags & flags)) {
			return result;
		}
		wi.xy = float2(angle_cs.x * wi.x - angle_cs.y * wi.y,
					   angle_cs.y * wi.x + angle_cs.x * wi.y);

		int2 sampled_lobe;
		float2 sampled_pdf;
		sampled_lobe = sample_lobe(wi.z, sampled_pdf, data.lambda[data.hero_wavelength_index], data.rand.xy);

		result.wo = diffract(wi, sampled_lobe, data.lambda[data.hero_wavelength_index]);

		if (!data.specular_microfacet.effectivelySmooth()) {
			mtl::Onb onb(h);
			result.wo = onb.to_world(result.wo);
		}

		if (result.wo.z < 0.0f) return result;
		result.pdf = max(2e-2f, sampled_pdf.x * sampled_pdf.y);
		result.throughput.flags = flags;
		result.throughput.val = color.spectral() * dir_albedo * lobe_intensity(sampled_lobe, abs(dot(h, result.wo)), data.lambda[data.hero_wavelength_index]);

		result.wo.xy = float2(angle_cs.x * result.wo.x + angle_cs.y * result.wo.y,
							  -angle_cs.y * result.wo.x + angle_cs.x * result.wo.y);
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		return 0.0f;
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 1.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		if (wi.z < 0) return 1.0f;
		return 1.0f - dir_albedo;
	}
	float3 energy(float3 wi, auto& data) const {
		if (data.sampling_flags & flags) {
			return color.color(data.spectrumed) * dir_albedo;
		}
		return 0.0f;
	}
};

}// namespace mtl