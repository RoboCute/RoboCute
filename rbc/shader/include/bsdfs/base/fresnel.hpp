#pragma once
#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>

#include <std/utility>

namespace mtl {

using namespace luisa::shader;

template<class T>
concept Fresnel = requires(std::remove_cvref_t<T> const f, float cos_theta) {
	f.rsrp(cos_theta);
	f.RsRp(cos_theta);
	f.unpolarized(cos_theta);
};

class DielectricFresnel {

	static float energy_fit(float eta) {
		return log((10893.0f * eta - 1438.2f) / (-774.4f * sqr(eta) + 10212.0f * eta + 1.0f));
	}

public:
	float ior;

	constexpr DielectricFresnel() = default;

	constexpr explicit DielectricFresnel(float ior) : ior(ior) {}

	void init(float ior) {
		this->ior = ior;
	}

	std::pair<float, float> rsrp(float cos_theta) const {
		float eta = ior;
		if (cos_theta < 0.0f) {
			cos_theta = -cos_theta;
			eta = rcp(eta);
		}
		float t0 = sqr(cos_theta) + sqr(eta) - 1.0f;
		// if (t0 <= 0.0f) {
		// 	return 1.0f
		// }
		// t0 = sqrt(t0);
		t0 = t0 > 0.0f ? sqrt(t0) / eta : 0.0f;
		float t1 = eta * t0;
		float t2 = eta * cos_theta;

		float rs = (cos_theta - t1) / (cos_theta + t1);
		float rp = (t0 - t2) / (t0 + t2);

		return {rs, rp};
	}
	std::pair<float, float> RsRp(float cos_theta) const {
		auto R = rsrp(cos_theta);
		return {sqr(R.first), sqr(R.second)};
	}
	float unpolarized(float cos_theta) const {
		auto R = RsRp(cos_theta);
		return 0.5f * (R.first + R.second);
	}

	class RefractResult {
		float3 wi;
		float3 n;
		float inv_eta;
		float cos_theta;
		float t0;

	public:
		RefractResult(float3 wi, float3 n, float inv_eta, float cos_theta, float t0)
			: wi(wi),
			  n(n),
			  inv_eta(inv_eta),
			  cos_theta(cos_theta),
			  t0(t0) {}

		bool valid() const {
			return t0 > 0.0f;
		}
		float3 out() const {
			return inv_eta * wi - (inv_eta * cos_theta + sqrt(t0)) * n;
		}
	};

	RefractResult refract(float3 wi, float3 n) const {
		float cos_theta = dot(n, wi);
		float inv_eta;
		if (cos_theta < 0) {
			inv_eta = rcp(ior);
		} else {
			cos_theta = -cos_theta;
			n = -n;
			inv_eta = ior;
		}
		return RefractResult{wi, n, inv_eta, cos_theta, 1.0f - (1.0f - sqr(cos_theta)) * sqr(inv_eta)};
	}

	float energy() const {
		if (ior > 1.0f)
			return energy_fit(ior);
		else if (ior < 1.0f)
			return 1.0f - sqr(ior) * (1.0f - energy_fit(rcp(ior)));
		else
			return 0.0f;
	}
};

class SchlickF82tintFresnel {
public:
	float3 f0;
	float3 f82;
	float3 f90;
	float weight;

	constexpr SchlickF82tintFresnel() = default;

	void init(float3 f0, float3 f82, float3 f90, float weight = 1.0f) {
		this->f0 = f0;
		this->f82 = f82;
		this->f90 = f90;
		this->weight = weight;
	}

	float3 unpolarized(float cos_theta) const {
		constexpr float const COS_THETA_MAX = 1.0f / 7.0f;
		constexpr float const COS_THETA_FACTOR = 1.0f / (COS_THETA_MAX * pow6(1.0f - COS_THETA_MAX));

		float3 a = lerp(f0, f90, pow5(1.0f - COS_THETA_MAX)) * (float3(1.0f) - f82) * COS_THETA_FACTOR;
		return saturate((lerp(f0, f90, pow5(1.0f - cos_theta)) - a * cos_theta * pow6(1.0f - cos_theta)) * weight);
	}
	std::pair<float3, float3> RsRp(float cos_theta) const {
		float3 f = unpolarized(cos_theta);
		return {f, f};
	}
	std::pair<float3, float3> rsrp(float cos_theta) const {
		float3 f = sqrt(unpolarized(cos_theta));
		return {f, f};
	}
};

// A Practical Extension to Microfacet Theory for the Modeling of Varying Iridescence
// https://belcour.github.io/blog/research/publication/2017/05/01/brdf-thin-film.html
template<Fresnel Base>
class ThinFilmFresnel {
	static float3 eval_sensitivity(float opd, float3 shift) {
		// Use Gaussian fits, given by 3 parameters: val, pos and var
		float phase = 2.0f * pi * opd;
		float3 val = float3(5.4856e-13f, 4.4201e-13f, 5.2481e-13f);
		float3 pos = float3(1.6810e+06f, 1.7953e+06f, 2.2084e+06f);
		float3 var = float3(4.3278e+09f, 9.3046e+09f, 6.6121e+09f);
		float3 xyz = val * sqrt(2.0f * pi * var) * cos(pos * phase + shift) * exp(-var * phase * phase);
		xyz.x += 9.7470e-14f * sqrt(2.0 * pi * 4.5282e+09) * cos(2.2399e+06f * phase + shift[0]) * exp(-4.5282e+09f * phase * phase);
		return xyz / 1.0685e-7f;
	}

	static float3 f0_to_ior(float3 F0) {
		float3 sqrtF0 = sqrt(clamp(F0, 0.0f, 0.999f));
		return (1.0f + sqrtF0) / (1.0f - sqrtF0);
	}

	static float2 complex_mul(float2 z1, float2 z2) { return float2(z1.x * z2.x - z1.y * z2.y, z1.x * z2.y + z1.y * z2.x); }
	static float2 complex_conj(float2 z) { return float2(z.x, -z.y); }
	static float2 complex_inv(float2 z) { return complex_conj(z) / complex_mag2(z); }
	static float2 complex_add(float a, float2 z) { return float2(a + z.x, z.y); }
	static float2 complex_sub(float a, float2 z) { return float2(a - z.x, -z.y); }
	static float complex_mag2(float2 z) { return sqr(z.x) + sqr(z.y); }

public:
	float thickness;
	DielectricFresnel film;
	Base base;
	float3 lambda_nm;

	void init(float thickness, float tf_ior, float3 lambda_nm) {
		this->thickness = thickness;
		this->film.init(tf_ior);
		this->lambda_nm = lambda_nm;
	}

	constexpr ThinFilmFresnel() = default;

	std::pair<float3, float3> RsRp(float cos_theta) const {
		if (thickness <= 0.0f) {
			auto R = base.RsRp(cos_theta);
			return {R.first, R.second};
		}
		float cos_theta_base = sqrt(1.0f - (1.0f - sqr(cos_theta)) * sqr(rcp(film.ior)));

		// Optical path difference
		float distMeters = thickness * 1e-6f;// μm
		float opd = 2.0 * film.ior * abs(cos_theta_base) * distMeters;

		// Phase shift
		float cosB = rsqrt(sqr(film.ior) + 1.0f);
		float2 phi21 = float2(pi, abs(cos_theta) < cosB ? 0.0f : pi);
		float3 phi23s = 0.0f;
		float3 phi23p = 0.0f;

		if constexpr (requires() {
						  base.ior;
					  }) {
			phi23s = (base.ior < film.ior) ? pi : 0.0f;
		} else if constexpr (requires() {
								 base.f0;
							 }) {
			phi23s = select(float3(pi), float3(0.0f), f0_to_ior(base.f0) < film.ior);
		}
		phi23p = phi23s;

		// Iridescence term
		float3 Is = 0.0f;
		float3 Ip = 0.0f;

#ifdef USE_SPECTRUM
		// First interface
		float2 r12;
		{
			auto r = film.rsrp(cos_theta);
			r12.x = r.first;
			r12.y = r.second;
		}
		float2 t12 = 1.0f - r12;

		float2 r21;
		{
			auto r = film.rsrp(-cos_theta_base);
			r21.x = r.first;
			r21.y = r.second;
		}
		float2 t21 = 1.0f - r21;

		r21 = ite(phi21 > 0.5f * pi, -r21, r21);

		// Second interface
		float3 r23s;
		float3 r23p;
		{
			auto r = base.rsrp(cos_theta_base);
			r23s = r.first;
			r23p = r.second;
		}

		r23s = ite(phi23s > 0.5f * pi, -r23s, r23s);
		r23p = ite(phi23p > 0.5f * pi, -r23p, r23p);

		float3 dphi = 2.0f * pi * (opd * 1e+9f) / lambda_nm;

		// Thus compute final Fresnel reflection, accounting for all bounces in the thin-film
		for (int c = 0; c <= 2; ++c) {
			// complex phase shift due to optical path difference:
			float2 expidphi(cos(dphi[c]), sin(dphi[c]));
			// Implements formula (3) of Belcour & Barla, using complex arithmetic
			Is[c] = complex_mag2(
				complex_add(r12.x,
							t12.x * r23s[c] * t21.x *
								complex_mul(expidphi,
											complex_inv(
												complex_sub(1.0f,
															r21.x * r23s[c] * expidphi)))));
			Ip[c] = complex_mag2(
				complex_add(r12.y,
							t12.y * r23p[c] * t21.y *
								complex_mul(expidphi,
											complex_inv(
												complex_sub(1.0f,
															r21.y * r23p[c] * expidphi)))));
		}
#else
		// First interface
		float2 R12;
		{
			auto r = film.RsRp(cos_theta);
			R12.x = r.first;
			R12.y = r.second;
		}
		float2 T121 = 1.0f - R12;

		// Second interface
		float3 R23s;
		float3 R23p;
		{
			auto r = base.RsRp(cos_theta_base);
			R23s = r.first;
			R23p = r.second;
		}
		float3 r123s = sqrt(R12.x * R23s);
		float3 r123p = sqrt(R12.y * R23p);

		float3 Cm, Sm;

		// Iridescence term using spectral antialiasing for Perpendicular polarization

		// Reflectance term for m=0 (DC term amplitude)
		float3 Rs = (sqr(T121.x) * R23s) / (float3(1.0) - R12.x * R23s);
		Is += R12.x + Rs;

		// Reflectance term for m>0 (pairs of diracs)
		Cm = Rs - T121.x;
		for (int m = 1; m <= 2; m++) {
			Cm *= r123s;
			Sm = 2.0f * eval_sensitivity(float(m) * opd, float(m) * (phi23s + float3(phi21.x)));
			Is += Cm * Sm;
		}

		// Iridescence term using spectral antialiasing for Parallel polarization

		// Reflectance term for m=0 (DC term amplitude)
		float3 Rp = (sqr(T121.y) * R23p) / (float3(1.0) - R12.y * R23p);
		Ip += R12.y + Rp;

		// Reflectance term for m>0 (pairs of diracs)
		Cm = Rp - T121.y;
		for (int m = 1; m <= 2; m++) {
			Cm *= r123p;
			Sm = 2.0f * eval_sensitivity(float(m) * opd, float(m) * (phi23p + float3(phi21.y)));
			Ip += Cm * Sm;
		}
		// Convert back to RGB reflectance
		Is = spectrum::xyz_E_to_rec2020_D65(Is);
		Ip = spectrum::xyz_E_to_rec2020_D65(Ip);
#endif
		return {saturate(Is), saturate(Ip)};
	}
	std::pair<float3, float3> rsrp(float cos_theta) const {
		auto R = RsRp(cos_theta);
		return {sqrt(R.first), sqrt(R.second)};
	}
	float3 unpolarized(float cos_theta) const {
		auto R = RsRp(cos_theta);
		return 0.5f * (R.first + R.second);
	}
};

}// namespace mtl