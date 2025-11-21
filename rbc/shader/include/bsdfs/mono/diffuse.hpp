#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>

namespace mtl {

using namespace luisa::shader;

// EON: A practical energy-preserving rough diffuse BRDF
// https://jcgt.org/published/0014/01/06/
class EnergyPreservingOrenNayarBRDF {
	spectrum::SpectrumColor color;
	float base_weight;
	float roughness;

	static constexpr float const constant1_FON = 0.5f - 2.0f / (3.0f * pi);
	static constexpr float const constant2_FON = 2.0f / 3.0f - 28.0f / (15.0f * pi);

	static float E_FON_approx(float mu, float r) {
		float mucomp = 1.0f - mu;
		constexpr float const g1 = 0.0571085289f;
		constexpr float const g2 = 0.491881867f;
		constexpr float const g3 = -0.332181442f;
		constexpr float const g4 = 0.0714429953f;
		float GoverPi = mucomp * (g1 + mucomp * (g2 + mucomp * (g3 + mucomp * g4)));
		return (1.0f + r * GoverPi) / (1.0f + constant1_FON * r);
	}
	static float E_FON_approx2(float mu, float r) {
		float mucomp = 1.0f - mu;
		constexpr float const g1 = 0.0571085289f;
		constexpr float const g2 = 0.491881867f;
		constexpr float const g3 = -0.332181442f;
		constexpr float const g4 = 0.0714429953f;
		float GoverPi = mucomp * (g1 + mucomp * (g2 + mucomp * (g3 + mucomp * g4)));
		return (1.0f + r * GoverPi);
	}

	static float3x3 orthonormal_basis_ltc(float3 w) {
		float len_sqr = dot(w.xy, w.xy);
		float3 X = len_sqr > 0.0f ? float3(w.x, w.y, 0.0f) * rsqrt(len_sqr) : float3(1, 0, 0);
		float3 Y = float3(-X.y, X.x, 0.0f);// cross(Z, X)
		return float3x3(X, Y, float3(0, 0, 1));
	}

	static void ltc_coeffs(float mu, float r,
						   float& a, float& b, float& c, float& d) {
		a = 1.0f + r * (0.303392f + (-0.518982f + 0.111709f * mu) * mu + (-0.276266f + 0.335918f * mu) * r);
		b = r * (-1.16407f + 1.15859f * mu + (0.150815f - 0.150105f * mu) * r) / (mu * mu * mu - 1.43545f);
		c = 1.0f + r * (0.20013f + (-0.506373f + 0.261777f * mu) * mu);
		d = r * (0.540852f + (-1.01625f + 0.475392f * mu) * mu) / (-1.0743f + (0.0725628f + mu) * mu);
	}

	static float4 cltc_sample(float3 wi_local, float r, float2 u) {
		float a, b, c, d;
		ltc_coeffs(wi_local.z, r, a, b, c, d);
		float R = sqrt(u.x);
		float phi = 2.0f * pi * u.y;
		float x = R * cos(phi);
		float y = R * sin(phi);
		float vz = 1.0f / sqrt(d * d + 1.0f);
		float s = 0.5f * (1.0f + vz);
		x = -lerp(sqrt(1.0f - y * y), x, s);
		float3 wh = float3(x, y, sqrt(max(1.0f - (x * x + y * y), 0.0f)));
		float pdf_wh = wh.z / (pi * s);
		float3 wo = float3(a * wh.x + b * wh.z, c * wh.y, d * wh.x + wh.z);
		float len = length(wo);
		float detM = c * (a - b * d);
		float pdf_wi = pdf_wh * len * len * len / detM;
		float3x3 fromLTC = orthonormal_basis_ltc(wi_local);
		wo = normalize(fromLTC * wo);
		return float4(wo, pdf_wi);
	}

	static float cltc_pdf(float3 wi_local, float3 wo_local, float r) {
		float3x3 toLTC = transpose(orthonormal_basis_ltc(wi_local));
		float3 wo = toLTC * wo_local;
		float a, b, c, d;
		ltc_coeffs(wi_local.z, r, a, b, c, d);
		float detM = c * (a - b * d);
		float3 wh = float3(c * (wo.x - b * wo.z), (a - b * d) * wo.y, -c * (d * wo.x - a * wo.z));
		float len_sqr = dot(wh, wh);
		float vz = 1.0f / sqrt(d * d + 1.0f);
		float s = 0.5f * (1.0f + vz);
		float pdf = detM * detM / (len_sqr * len_sqr) * max(wh.z, 0.0f) / (pi * s);
		return pdf;
	}

	static float uniform_pdf(float mu, float r) {
		return pow(r, 0.1f) * (0.162925f + (-0.372058f + (0.538233f - 0.290822f * mu) * mu) * mu);
	}

public:
	static constexpr BSDFFlags const flags = BSDFFlags::DiffuseReflection;

	EnergyPreservingOrenNayarBRDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		color = p.base.color;
		base_weight = bp.weight.base;
		roughness = bp.weight.diffuse_roughness;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result;
		float mu_i = wi.z;
		float mu_o = wo.z;
		if (mu_i < 0.0f || mu_o < 0.0f || base_weight <= 0.0f || !(data.sampling_flags & flags)) {
			return result;
		}
		float s = dot(wi, wo) - mu_i * mu_o;
		float sovertF = s > 0.0f ? s / max(mu_i, mu_o) : s;
		float AF = 1.0f / (1.0f + constant1_FON * roughness);
		float3 f_ss = color.spectral() * AF * (1.0f + roughness * sovertF);
		float EFo = E_FON_approx2(mu_o, roughness) * AF;
		float EFi = E_FON_approx2(mu_i, roughness) * AF;
		float avgEF = AF * (1.0f + constant2_FON * roughness);
		float3 color_ms = sqr(color.spectral()) * avgEF / (float3(1.0f) - color.spectral() * (1.0f - avgEF));
		constexpr float const eps = 1.0e-7f;
		float3 f_ms = color_ms * (1.0f - EFo) * (1.0f - EFi) / max(eps, 1.0f - avgEF);
		result.val = (f_ss + f_ms) * inv_pi * mu_o * base_weight;
		result.flags = flags;
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		BSDFSample result;
		if (wi.z < 0.0f || base_weight <= 0.0f || !(data.sampling_flags & flags)) {
			return result;
		}
		float P_u = uniform_pdf(wi.z, roughness);
		float pdf_c;
		if (data.rand.z <= P_u) {
			result.wo = uniform_sample_hemisphere(data.rand.xy);
			pdf_c = cltc_pdf(wi, result.wo, roughness);
		} else {
			auto w = cltc_sample(wi, roughness, data.rand.xy);
			result.wo = w.xyz;
			pdf_c = w.w;
		}
		constexpr float const pdf_u = 1.0f / (2.0f * pi);
		result.pdf = lerp(pdf_c, pdf_u, P_u);
		result.throughput = eval(wi, result.wo, data);
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		if (wi.z < 0.0f || wo.z <= 0.0f || base_weight <= 0.0f || !(data.sampling_flags & flags))
			return 0.0f;
		float pdf_c = cltc_pdf(wi, wo, roughness);
		constexpr float const pdf_u = 1.0f / (2.0f * pi);
		return lerp(pdf_c, pdf_u, uniform_pdf(wi.z, roughness));
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 0.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		return 0.0f;
	}
	float3 energy(float3 wi, auto& data) const {
		if (!(data.sampling_flags & flags) || base_weight <= 0.0f) {
			return 0.0f;
		}
		float mu_i = wi.z;
		float EF = E_FON_approx(mu_i, roughness);
		float AF = 1.0f / (1.0f + constant1_FON * roughness);
		float avgEF = AF * (1.0f + constant2_FON * roughness);
		auto ss_color = color.color(data.spectrumed);
		float3 color_ms = sqr(ss_color) * avgEF / (float3(1.0f) - ss_color * (1.0f - avgEF));
		return lerp(color_ms, ss_color, EF) * base_weight;
	}
};

class SmoothLambertianBRDF {
	spectrum::SpectrumColor color;
	float base_weight;

public:
	static constexpr BSDFFlags const flags = BSDFFlags::DiffuseReflection;

	SmoothLambertianBRDF() = default;
	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		color = p.base.color;
		base_weight = bp.weight.base;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result;
		float mu_i = wi.z;
		float mu_o = wo.z;
		if (mu_i < 0.0f || mu_o < 0.0f || !(data.sampling_flags & flags) || base_weight <= 0.0f) {
			return result;
		}
		result.val = color.spectral() * (base_weight * inv_pi * mu_o);
		result.flags = flags;
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		float2 rand = data.rand.xy;
		BSDFSample result;
		if (wi.z < 0.0f || !(data.sampling_flags & flags) || base_weight <= 0.0f) {
			return result;
		}
		result.wo = cosine_sample_hemisphere(rand);
		result.pdf = inv_pi * result.wo.z;
		result.throughput.flags = flags;
		result.throughput.val = color.spectral() * (base_weight * inv_pi * result.wo.z);
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		return (wi.z >= 0.0f && wo.z > 0.0f && base_weight > 0.0f) ? inv_pi * wo.z : 0.0f;
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 0.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		return 0.0f;
	}
	float3 energy(float3 wi, auto& data) const {
		if (wi.z < 0.0f || !(data.sampling_flags & flags) || base_weight <= 0.0f) {
			return 0.0f;
		}
		return color.color(data.spectrumed) * base_weight;
	}
};

using DiffuseBRDF = EnergyPreservingOrenNayarBRDF;

}// namespace mtl