#pragma once
#include "aces.hpp"
using namespace luisa::shader;
struct ParamsLogC {
	float cut;
	float a, b, c, d, e, f;
};
inline ParamsLogC get_LogC() {
	ParamsLogC c;
	c.cut = 0.011361;
	c.a = 5.555556;
	c.b = 0.047996;
	c.c = 0.244161;
	c.d = 0.386036;
	c.e = 5.301883;
	c.f = 0.092819;
	return c;
}
static float LinearToLogC_Precise(float x) {
	auto LogC = get_LogC();
	float o;
	if (x > LogC.cut)
		o = LogC.c * log10(LogC.a * x + LogC.b) + LogC.d;
	else
		o = LogC.e * x + LogC.f;
	return o;
}

static float3 LinearToLogC(float3 x) {
	return float3(
		LinearToLogC_Precise(x.x),
		LinearToLogC_Precise(x.y),
		LinearToLogC_Precise(x.z));
}

static float LogCToLinear_Precise(float x) {
	auto LogC = get_LogC();
	float o;
	if (x > LogC.e * LogC.cut + LogC.f)
		o = (pow(10.0, (x - LogC.d) / LogC.c) - LogC.b) / LogC.a;
	else
		o = (x - LogC.f) / LogC.e;
	return o;
}
static float3 LogCToLinear(float3 x) {
	return float3(
		LogCToLinear_Precise(x.x),
		LogCToLinear_Precise(x.y),
		LogCToLinear_Precise(x.z));
}
static float3 Contrast(float3 c, float midpoint, float contrast) {
	return (c - midpoint) * contrast + midpoint;
}
static const float3x3 LIN_2_LMS_MAT = {
	3.90405e-1, 5.49941e-1, 8.92632e-3,
	7.08416e-2, 9.63172e-1, 1.35775e-3,
	2.31082e-2, 1.28021e-1, 9.36245e-1};

static const float3x3 LMS_2_LIN_MAT = {
	2.85847e+0, -1.62879e+0, -2.48910e-2,
	-2.10182e-1, 1.15820e+0, 3.24281e-4,
	-4.18120e-2, -1.18169e-1, 1.06867e+0};

static float3 WhiteBalance(float3 c, float3 balance) {
	float3 lms = LIN_2_LMS_MAT * c;
	lms *= balance;
	return LMS_2_LIN_MAT * lms;
}
static float3 ChannelMixer(float3 c, float3 red, float3 green, float3 blue) {
	return float3(
		dot(c, red),
		dot(c, green),
		dot(c, blue));
}
static float3 LiftGammaGainHDR(float3 c, float3 lift, float3 invgamma, float3 gain) {
	c = c * gain + lift;

	// ACEScg will output negative values, as clamping to 0 will lose precious information we'll
	// mirror the gamma function instead
	return FastSign(c) * pow(abs(c), invgamma);
}
static float3 RgbToHsv(float3 c) {
	float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
	float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	float e = 1e-4;
	return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
static float rec2020_Luminance(float3 linearRgb) {
	// We use REC2020 color space
	return dot(linearRgb, float3(0.262700, 0.677998, 0.059302));
}
static float Luminance(float3 linearRgb) {
	return dot(linearRgb, float3(0.2126729, 0.7151522, 0.0721750));
}
static float RotateHue(float value, float low, float hi) {
	return (value < low) ? value + hi : (value > hi) ? value - hi :
													   value;
}
static float3 Saturation(float3 c, float sat) {
	float luma = Luminance(c);
	return float3(luma) + sat * (c - luma);
}

static float3 HsvToRgb(float3 c) {
	float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	float3 p = abs(fract(c.xxx + K.xyz) * float3(6.0) - K.www);
	return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}
static float4 QuadraticThreshold(float4 color, float threshold, float3 curve) {
	// Pixel brightness
	float br = Max3(color.r, color.g, color.b);

	// Under-threshold part: quadratic curve
	float rq = clamp(br - curve.x, 0.0, curve.y);
	rq = curve.z * rq * rq;

	// Combine and apply the brightness response curve.
	color *= max(rq, br - threshold) / max(br, 1e-4f);

	return color;
}

static float3 SafeHDR(float3 c) {
	return min(c, float3(65472.0));
}

static float4 SafeHDR(float4 c) {
	return min(c, float4(65472.0));
}

static float4 DownsampleBox13Tap(SampleImage& img, float2 uv, float2 texelSize) {
	float4 A = img.sample((uv + texelSize * float2(-1.0, -1.0)), Filter::POINT, Address::EDGE);
	float4 B = img.sample((uv + texelSize * float2(0.0, -1.0)), Filter::POINT, Address::EDGE);
	float4 C = img.sample((uv + texelSize * float2(1.0, -1.0)), Filter::POINT, Address::EDGE);
	float4 D = img.sample((uv + texelSize * float2(-0.5, -0.5)), Filter::LINEAR_POINT, Address::EDGE);
	float4 E = img.sample((uv + texelSize * float2(0.5, -0.5)), Filter::LINEAR_POINT, Address::EDGE);
	float4 F = img.sample((uv + texelSize * float2(-1.0, 0.0)), Filter::POINT, Address::EDGE);
	float4 G = img.sample((uv), Filter::POINT, Address::EDGE);
	float4 H = img.sample((uv + texelSize * float2(1.0, 0.0)), Filter::POINT, Address::EDGE);
	float4 I = img.sample((uv + texelSize * float2(-0.5, 0.5)), Filter::LINEAR_POINT, Address::EDGE);
	float4 J = img.sample((uv + texelSize * float2(0.5, 0.5)), Filter::LINEAR_POINT, Address::EDGE);
	float4 K = img.sample((uv + texelSize * float2(-1.0, 1.0)), Filter::POINT, Address::EDGE);
	float4 L = img.sample((uv + texelSize * float2(0.0, 1.0)), Filter::POINT, Address::EDGE);
	float4 M = img.sample((uv + texelSize * float2(1.0, 1.0)), Filter::POINT, Address::EDGE);

	float2 div = float2(1.0 / 4.0) * float2(0.5, 0.125);

	float4 o = (D + E + I + J) * div.x;
	o += (A + B + G + F) * div.y;
	o += (B + C + H + G) * div.y;
	o += (F + G + L + K) * div.y;
	o += (G + H + M + L) * div.y;

	return o;
}

static float4 UpsampleTent(SampleImage& img, float2 uv, float2 texelSize, float4 sampleScale) {
	float4 d = float4(texelSize, texelSize) * float4(1.0, 1.0, -1.0, 0.0) * sampleScale;
	float4 s = img.sample((uv - d.xy), Filter::LINEAR_POINT, Address::EDGE);
	s += img.sample((uv - d.wy), Filter::LINEAR_POINT, Address::EDGE) * 2.0f;
	s += img.sample((uv - d.zy), Filter::LINEAR_POINT, Address::EDGE);
	s += img.sample((uv + d.zw), Filter::LINEAR_POINT, Address::EDGE) * 2.0f;
	s += img.sample((uv), Filter::LINEAR_POINT, Address::EDGE) * 4.0f;
	s += img.sample((uv + d.xw), Filter::LINEAR_POINT, Address::EDGE) * 2.0f;
	s += img.sample((uv + d.zy), Filter::LINEAR_POINT, Address::EDGE);
	s += img.sample((uv + d.wy), Filter::LINEAR_POINT, Address::EDGE) * 2.0f;
	s += img.sample((uv + d.xy), Filter::LINEAR_POINT, Address::EDGE);
	return s * (1.0f / 16.0f);
}

#define LUT_SPACE_ENCODE(x) LinearToLogC(x)
#define LUT_SPACE_DECODE(x) LogCToLinear(x)