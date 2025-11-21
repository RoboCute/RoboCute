#pragma once
#include <luisa/std.hpp>
#include <spectrum/spectrum_args.hpp>
#include <spectrum/color_space.hpp>
#include <utils/heap_indices.hpp>

#define USE_SPECTRUM

namespace spectrum {
using namespace luisa::shader;

namespace detail {
inline float3 tristimulus_to_coefficients(BindlessVolume& heap, float3 rgb) {
	uint maxc = (rgb.x > rgb.y) ? ((rgb.x > rgb.z) ? 0 : 2) :
								  ((rgb.y > rgb.z) ? 1 : 2);
	float z = max(1e-6f, rgb[maxc]);
	float x = rgb[(maxc + 1) % 3] / z;
	float y = rgb[(maxc + 2) % 3] / z;
	z = inverse_smoothstep(inverse_smoothstep(z));
	float3 size(spectrum_lut3d_res);
	float3 uvw = float3(x, y, z) * ((size - 1.0f) / size) + float3(0.5f) / size;
	uvw.x += float(maxc);
	uvw.x *= 1.0f / 3.0f;
	return heap.uniform_idx_volume_sample(heap_indices::spectrum_lut3d_idx, uvw, Filter::LINEAR_POINT, Address::EDGE).xyz;
}
inline float spectrum_reflectance(float lambda, float3 coeff) {
	float x = fma(fma(coeff[0], lambda, coeff[1]), lambda, coeff[2]);
	float y = rsqrt(fma(x, x, 1.f));
	return fma(.5f * x, y, .5f);
}
inline float d65_normalized(BindlessImage& heap, float lambda) {
	float size = illum_d65_size;
	auto radiance = heap.uniform_idx_image_sample(heap_indices::illum_d65_idx, float2((lambda - wavelength_min) / (wavelength_max - wavelength_min) * ((size - 1.0f) / size) + 0.5f / size, 0.5f), Filter::LINEAR_POINT, Address::EDGE).x;
	return radiance;
}
// log of Gaussian function
float gaussdistlog(float x, float3 p) {
	x = (x - p.y) / p.z;
	return p.x - 0.5 * x * x;
}
// Sigmoid function
float gtanh(float x, float3 p) {
	return p.x / (1 + exp(p.z * (p.y - x)));
}
// CIE 1931 2° 5-bell
float cmf_s_1931_5b(float wl) {
	auto param_s = std::array<float3, 5>{
		float3{0.728039840518760, 452.5373300487677, 20.63998741455027},
		float3{-0.903768754848100, 436.5072798146079, 6.80213865708983},
		float3{-1.142203568915948, 426.6964514807404, 5.82897816302603},
		float3{0.418410092402545, 567.5101202778363, 43.44030592589013},
		float3{-0.567908922578286, 606.9866079746189, 25.57205199243601}};
	float result = 0.0f;
	for (int i = 0; i < param_s.size(); ++i) {
		float x = gaussdistlog(wl, param_s[i]);
		result += exp(x);
	}
	return result;
}
float3 xy_1931_5b(float wl) {
	auto param_y = std::array<float3, 3>{
		float3{1.606156428203371, 502.0075537941163, 0.0967029313067696},
		float3{-0.800580541463583, 513.8824977974497, 0.0881332765655076},
		float3{-0.550389264835406, 572.0735758547771, 0.0499662323614685}};
	auto param_z = std::array<float3, 2>{
		float3{0.878531722389920, 500.8587238060737, -0.1076130311623401},
		float3{-0.056933268287903, 470.2562688668381, -0.0756868208907367}};

	float y = 0.007958436344230262f;
	for (int i = 0; i < param_y.size(); ++i) {
		y += gtanh(wl, param_y[i]);
	}
	float z = 0.0f;
	for (int i = 0; i < param_z.size(); ++i) {
		z += gtanh(wl, param_z[i]);
	}
	return float3{1 - (y + z), y, z};
}
float3 cmf_1931_5b(float wl) {
	return xy_1931_5b(wl) * cmf_s_1931_5b(wl);
}
}// namespace detail
#ifdef USE_SPECTRUM
constexpr bool const use_spectrum = true;
#else
constexpr bool const use_spectrum = false;
#endif
inline float3 sample_xyz(BindlessImage& heap, float dx) {
	float size = cie_xyz_cdfinv_size;
	return heap.uniform_idx_image_sample(heap_indices::cie_xyz_cdfinv_idx, float2(dx * ((size - 1.0f) / size) + 0.5f / size, 0.5f), Filter::LINEAR_POINT, Address::EDGE).xyz;
}

inline void modify_throughput(BindlessImage& image_heap, SpectrumArg& args, float3& throughput, float3& last_throughput, float3& di_result) {
	float3 xyz_inv_pdf(detail::xy_1931_5b(args.lambda[args.hero_index]) * 3.0f);
#ifdef USE_SPECTRUM
	args.lambda = args.lambda[args.hero_index];
	last_throughput = last_throughput[args.hero_index];
	di_result = di_result[args.hero_index] * xyz_inv_pdf;
	throughput = throughput[args.hero_index] * xyz_inv_pdf;
#else
	xyz_inv_pdf *= detail::d65_normalized(image_heap, args.lambda[args.hero_index]);
	float3 inv_wavelength_pdf = max(float3(0.0f), spectrum::xyz_to_rec2020(xyz_inv_pdf)) *
								float3(0.92150449687, 0.9387315829, 0.987215281555);
	throughput *= inv_wavelength_pdf;
#endif
}

inline float3 reflectance_to_spectrum(BindlessVolume& heap, SpectrumArg& args, float3 x) {
#ifdef USE_SPECTRUM
	auto coeff = detail::tristimulus_to_coefficients(heap, x);
	return float3(detail::spectrum_reflectance(args.lambda.x, coeff), detail::spectrum_reflectance(args.lambda.y, coeff), detail::spectrum_reflectance(args.lambda.z, coeff));
#else
	return x;
#endif
}

inline float3 emission_to_spectrum(BindlessImage& image_heap, BindlessVolume& volume_heap, SpectrumArg& args, float3 x) {
#ifdef USE_SPECTRUM
	// set max color to 50% will get a much smooth result
	float max_emission = max(1e-4f, reduce_max(x)) * 2.0f;
	auto color = reflectance_to_spectrum(volume_heap, args, x / max_emission);
	return max_emission * color * float3(detail::d65_normalized(image_heap, args.lambda.x), detail::d65_normalized(image_heap, args.lambda.y), detail::d65_normalized(image_heap, args.lambda.z));
#else
	return x;
#endif
}

inline float3 spectrum_to_tristimulus(float3 x) {
#ifdef USE_SPECTRUM
	return spectrum::xyz_to_rec2020(x);
#else
	return x;
#endif
}

inline float spectrum_to_illuminance(float3 x) {
#ifdef USE_SPECTRUM
	return reduce_sum(x) * (1.0f / 3.0f);
#else
	return spectrum::rec2020_to_xyz(x).y;
#endif
}

inline float spectrum_to_weight(float3 x) {
	// TODO: What is the best heuristic to use here?
	// reduce_sum seems to perform slightly better sometimes, while reduce_max is better in others. Need to look at more cases to be sure
	// Also tried Luminance, but the result was very close to the plain sum
	return reduce_sum(x) * (1.0f / 3.0f);
}

class SpectrumColor {
	float3 origin_color;
#ifdef USE_SPECTRUM
	float3 spectral_color;
#endif

public:
	SpectrumColor() = default;

#ifdef USE_SPECTRUM
	SpectrumColor(float origin) : origin_color(origin), spectral_color(0.0f) {}

	SpectrumColor(float3 origin) : origin_color(origin), spectral_color(0.0f) {}

	SpectrumColor(float3 origin, float3 spectral) : origin_color(origin), spectral_color(spectral) {}
#else
	SpectrumColor(float origin) : origin_color(origin) {}

	SpectrumColor(float3 origin) : origin_color(origin) {}

	SpectrumColor(float3 origin, float3 spectral) : origin_color(origin) {}
#endif

	static SpectrumColor fromReflectance(float3 origin, BindlessVolume& heap, SpectrumArg& args) {
		return SpectrumColor(origin, reflectance_to_spectrum(heap, args, origin));
	}
	static SpectrumColor fromEmission(float3 origin, BindlessImage& image_heap, BindlessVolume& volume_heap, SpectrumArg& args) {
		return SpectrumColor(origin, emission_to_spectrum(image_heap, volume_heap, args, origin));
	}

	void init_reflectance(BindlessVolume& heap, SpectrumArg& args, float3x3 resource_to_rec2020) {
		origin_color = saturate(resource_to_rec2020 * origin_color);
#ifdef USE_SPECTRUM
		spectral_color = reflectance_to_spectrum(heap, args, origin_color);
#endif
	}
	void init_emission(BindlessImage& image_heap, BindlessVolume& volume_heap, SpectrumArg& args, float3x3 resource_to_rec2020) {
		origin_color = max(resource_to_rec2020 * origin_color, float3(0.f));
#ifdef USE_SPECTRUM
		spectral_color = emission_to_spectrum(image_heap, volume_heap, args, origin_color);
#endif
	}

	void operator*=(float3 other) {
		origin_color *= other;
#ifdef USE_SPECTRUM
		spectral_color *= other;
#endif
	}

	float3 origin() const {
		return origin_color;
	}
	float3 spectral() const {
#ifdef USE_SPECTRUM
		return spectral_color;
#else
		return origin_color;
#endif
	}
	float3 color(bool use_spectral = true) const {
#ifdef USE_SPECTRUM
		return use_spectral ? spectral_color : origin_color;
#else
		return origin_color;
#endif
	}
};

}// namespace spectrum
