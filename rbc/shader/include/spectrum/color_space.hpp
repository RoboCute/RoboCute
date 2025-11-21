#pragma once
#include <luisa/std.hpp>
namespace spectrum {
using namespace luisa::shader;
inline float3 linear_to_srgb(float3 x) {
	return ite(x <= 0.00031308f, 12.92f * x, 1.055f * pow(x, float3(1.0f / 2.4f)) - 0.055f);
}

inline float3 srgb_to_linear(float3 x) {
	return ite(x < 0.04045f, x / 12.92f, pow((x + 0.055f) / 1.055f, float3(2.4f)));
}

inline float3 srgb_to_acescg(float3 col) {
	float3x3 const mat = transpose(float3x3(0.61319, 0.33951, 0.04737,
											0.07021, 0.91634, 0.01345,
											0.02062, 0.10957, 0.86961));
	return mat * col;
}

inline float3 acescg_to_srgb(float3 col) {
	float3x3 const mat = transpose(float3x3(1.70505, -0.62179, -0.08326,
											-0.13026, 1.14080, -0.01055,
											-0.02400, -0.12897, 1.15297));
	return mat * col;
}
inline float3 aces_2065_1_to_srgb(float3 col) {
	float3x3 const mat = transpose(float3x3(0.4329401822f, 0.3754007997f, 0.1893982296f,
											0.0893519795f, 0.8165687816f, 0.1030198606f,
											0.0191311640f, 0.1181572406f, 0.9421850433f));
	return mat * col;
}
inline float3 srgb_to_aces_2065_1(float3 col) {
	float3x3 const mat = transpose(float3x3(2.5580960675f, -1.1193369237f, -0.3918145117f,
											-0.2777157499f, 1.3658939640f, -0.0935307506f,
											-0.0171119873f, -0.1485458837f, 1.0810484773f));
	return mat * col;
}
inline float3 rgb_to_ycocg(float3 rgb) {
	float3x3 const mat = transpose(float3x3(0.25f, 0.5f, 0.25f,
											-0.25f, 0.5f, -0.25f,
											0.5f, 0.f, -0.5));
	return mat * rgb;
}
inline float3 ycocg_to_rgb(float3 ycocg) {
	float3x3 const mat = transpose(float3x3(1.f, -1.f, 1.f,
											1.f, 1.f, 0.f,
											1.f, -1.f, -1));
	return mat * ycocg;
}
inline float3 xyz_to_srgb(float3 x) {
	float3x3 const mat = transpose(float3x3(
		3.2409699419045213f, -1.5373831775700935f, -0.4986107602930033f,
		-0.9692436362808798f, 1.8759675015077206f, 0.0415550574071756f,
		0.0556300796969936f, -0.2039769588889765f, 1.0569715142428784f));
	return mat * x;
}
inline float3 xyz_to_rec2020(float3 x) {
	float3x3 const mat = transpose(float3x3(
		1.716651, -0.355671, -0.253366,
		-0.666684, 1.616481, 0.015769,
		0.017640, -0.042771, 0.942103));
	return mat * x;
}
inline float3 xyz_E_to_rec2020_D65(float3 x) {
	float3x3 const mat = transpose(float3x3(
		1.668660, -0.432006, -0.236654,
		-0.690510, 1.673585, 0.016925,
		0.018993, -0.043268, 1.024276));
	return mat * x;
}
inline float3 srgb_to_xyz(float3 x) {
	float3x3 const mat = transpose(float3x3(
		0.412391, 0.357584, 0.180481,
		0.212639, 0.715169, 0.072192,
		0.019331, 0.119195, 0.950532));
	return mat * x;
}
inline float srgb_to_illuminance(float3 x) {
	return srgb_to_xyz(x).y;
}
inline float3 rec2020_to_xyz(float3 x) {
	float3x3 const mat = transpose(float3x3(
		0.636958, 0.144617, 0.168881,
		0.262700, 0.677998, 0.059302,
		0.000000, 0.028073, 1.060985));
	return mat * x;
}
inline float3 rec709_to_rec2020(float3 color) {
	float3x3 const conversion =
		transpose(float3x3(
			0.627402, 0.329292, 0.043306,
			0.069095, 0.919544, 0.011360,
			0.016394, 0.088028, 0.895578));
	return conversion * color;
}
inline float3 rec2020_to_rec709(float3 color) {
	float3x3 const conversion =
		transpose(float3x3(1.660491, -0.587641, -0.072850,
						   -0.124550, 1.132900, -0.008349,
						   -0.018151, -0.100579, 1.118730));
	return conversion * color;
}
inline float3 linear_to_st2084(float3 color) {
	float const m1 = 2610.f / 4096.f / 4.f;
	float const m2 = 2523.f / 4096.f * 128.f;
	float const c1 = 3424.f / 4096.f;
	float const c2 = 2413.f / 4096.f * 32.f;
	float const c3 = 2392.f / 4096.f * 32.f;
	float3 cp = pow(abs(color), m1);
	return pow((c1 + c2 * cp) / (1.0f + c3 * cp), m2);
}
inline float3 displayP3_to_rec2020(float3 color) {
	float3x3 const conversion =
		transpose(float3x3(
			0.753833, 0.198597, 0.047570,
			0.045744, 0.941777, 0.012479,
			-0.001210, 0.017602, 0.983609));
	return conversion * color;
}
inline float3 rec2020_to_displayP3(float3 color) {
	float3x3 const conversion =
		transpose(float3x3(
			1.343578, -0.282180, -0.061399,
			-0.065297, 1.075788, -0.010490,
			0.002822, -0.019598, 1.016777));
	return conversion * color;
}
}// namespace spectrum