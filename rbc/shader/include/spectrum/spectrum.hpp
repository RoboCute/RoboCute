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
}// namespace detail
#ifdef USE_SPECTRUM
constexpr bool const use_spectrum = true;
#else
constexpr bool const use_spectrum = false;
#endif
inline float3 sample_wavelengths(BindlessImage& heap, float dx) {
	float size = wavelength_lut_size;
	return heap.uniform_idx_image_sample(heap_indices::spectrum_wavelength_lut_idx, float2(dx * ((size - 1.0f) / size) + 0.5f / size, 0.5f), Filter::LINEAR_POINT, Address::EDGE).xyz;
}

inline float3 wavelength_pdf(BindlessImage& heap, float lambda) {
	float table_x = clamp(lambda - wavelength_min, 0.0f, float(wavelength_pdf_table_size - 1u));
	float inv_size = 1.0f / float(wavelength_lut_size);
	float3 u = (table_x + float3(0u, wavelength_pdf_table_size, wavelength_pdf_table_size * 2u) + 0.5f) * inv_size;
	return float3(
		heap.uniform_idx_image_sample(heap_indices::spectrum_wavelength_lut_idx, float2(u.x, 0.5f), Filter::LINEAR_POINT, Address::EDGE).w,
		heap.uniform_idx_image_sample(heap_indices::spectrum_wavelength_lut_idx, float2(u.y, 0.5f), Filter::LINEAR_POINT, Address::EDGE).w,
		heap.uniform_idx_image_sample(heap_indices::spectrum_wavelength_lut_idx, float2(u.z, 0.5f), Filter::LINEAR_POINT, Address::EDGE).w);
}

inline float3 tristimulus_to_accumulation(float3 x, SpectrumAccumulationArgs const& args) {
#ifdef USE_SPECTRUM
	return args.rec2020_to_accumulation * x;
#else
	return x;
#endif
}

inline float3 spectrum_to_accumulation(float3 x, SpectrumAccumulationArgs const& args) {
#ifdef USE_SPECTRUM
	return x * args.lane_scale;
#else
	return x;
#endif
}

inline float3 accumulation_to_tristimulus(float3 x, SpectrumAccumulationArgs const& args) {
#ifdef USE_SPECTRUM
	return args.accumulation_to_rec2020 * x;
#else
	return x;
#endif
}

inline void modify_throughput(BindlessImage& image_heap, SpectrumArg& args, SpectrumAccumulationArgs const& accumulation_args, float3& throughput, float3& last_throughput, float3& di_result) {
	float hero_lambda = args.lambda[args.hero_index];
	float3 p = wavelength_pdf(image_heap, hero_lambda);
	float q = max(reduce_sum(p) * (1.0f / 3.0f), 1e-20f);
	float3 inv_pdf_weight = p / q;
#ifdef USE_SPECTRUM
	args.lambda = hero_lambda;
	last_throughput = last_throughput[args.hero_index];
	di_result = di_result[args.hero_index] * inv_pdf_weight;
	throughput = throughput[args.hero_index] * inv_pdf_weight;
#else
	float d65 = detail::d65_normalized(image_heap, hero_lambda);
	float3 accumulation_weight = inv_pdf_weight * accumulation_args.lane_scale * d65;
	float3 rec2020_weight = max(float3(0.0f), accumulation_args.accumulation_to_rec2020 * accumulation_weight);
	throughput *= rec2020_weight;
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

inline float3 spectrum_to_tristimulus(float3 x, SpectrumAccumulationArgs const& args) {
#ifdef USE_SPECTRUM
	return accumulation_to_tristimulus(spectrum_to_accumulation(x, args), args);
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
