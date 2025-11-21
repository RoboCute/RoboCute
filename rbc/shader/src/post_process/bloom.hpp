#pragma once
#include <post_process/colors.hpp>

static float4 Prefilter(float4 color, float2 uv, float autoExposure, float _Params, float4 _Threshold) {
	color *= autoExposure;
	color = min(float4(_Params), color);// clamp to max
	color = QuadraticThreshold(color, _Threshold.x, _Threshold.yzw);
	return color;
}

static float4 FragPrefilter13(SampleImage& img, float autoExposure, float2 texcoord, float _Params, float4 _Threshold) {
	auto texel_size = float2(1) / float2(img.size());
	float4 color = DownsampleBox13Tap(img, texcoord, texel_size);
	return Prefilter(SafeHDR(color), texcoord, autoExposure, _Params, _Threshold);
}

static float4 FragDownsample13(SampleImage& img, float2 texcoord) {
	auto texel_size = float2(1) / float2(img.size());
	float4 color = DownsampleBox13Tap(img, texcoord, texel_size);
	return color;
}

static float4 Combine(SampleImage& img, float4 bloom, float2 uv) {
	float4 color = img.sample(uv, Filter::POINT, Address::EDGE);
	return bloom + color;
}

static float4 FragUpsampleTent(SampleImage& img, SampleImage& bloom_img, float2 texcoord, float _SampleScale) {
	auto texel_size = float2(1) / float2(img.size());
	float4 bloom = UpsampleTent(img, texcoord, texel_size, float4(_SampleScale));
	return Combine(bloom_img, bloom, texcoord);
}