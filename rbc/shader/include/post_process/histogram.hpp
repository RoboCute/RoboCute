#pragma once
#include <luisa/std.hpp>
#include <post_process/colors.hpp>
using namespace luisa::shader;

#define HISTOGRAM_BINS 128
#define HISTOGRAM_TEXELS HISTOGRAM_BINS / 4

#define HISTOGRAM_THREAD_X 16
#define HISTOGRAM_THREAD_Y 16
#define HISTOGRAM_REDUCTION_THREAD_X HISTOGRAM_THREAD_X
#define HISTOGRAM_REDUCTION_THREAD_Y HISTOGRAM_BINS / HISTOGRAM_THREAD_Y
#define HISTOGRAM_REDUCTION_ALT_PATH 0

#define HISTOGRAM_REDUCTION_BINS HISTOGRAM_REDUCTION_THREAD_X* HISTOGRAM_REDUCTION_THREAD_Y

static float GetHistogramBinFromLuminance(float value, float2 scaleOffset) {// 1/18   1/2
	return saturate(log2(value) * scaleOffset.x + scaleOffset.y);
}

static float GetLuminanceFromHistogramBin(float bin, float2 scaleOffset) {
	return exp2((bin - scaleOffset.y) / scaleOffset.x);
}

static float GetBinValue(Buffer<uint>& buffer, uint index, float maxHistogramValue) {
	return float(buffer.read(index)) * maxHistogramValue;
}

static float FindMaxHistogramValue(Buffer<uint>& buffer) {
	uint maxValue = 0u;

	for (uint i = 0; i < HISTOGRAM_BINS; i++) {
		uint h = buffer.read(i);
		maxValue = max(maxValue, h);
	}

	return float(maxValue);
}

static void FilterLuminance(Buffer<uint>& buffer, uint i, float maxHistogramValue, float2 scaleOffset, float4& filter) {
	float binValue = GetBinValue(buffer, i, maxHistogramValue);

	// Filter dark areas
	float offset = min(filter.z, binValue);
	binValue -= offset;
	filter.zw -= float2(offset);

	// Filter highlights
	binValue = min(filter.w, binValue);
	filter.w -= binValue;

	// Luminance at the bin
	float luminance = GetLuminanceFromHistogramBin(float(i) / float(HISTOGRAM_BINS), scaleOffset);

	filter.xy += float2(luminance * binValue, binValue);
}

static float GetAverageLuminance(Buffer<uint>& buffer, float4 params, float maxHistogramValue, float2 scaleOffset) {
	// Sum of all bins
	uint i;
	float totalSum = 0.0;

	for (i = 0; i < HISTOGRAM_BINS; i++)
		totalSum += GetBinValue(buffer, i, maxHistogramValue);

	// Skip darker and lighter parts of the histogram to stabilize the auto exposure
	// x: filtered sum
	// y: accumulator
	// zw: fractions
	float4 filter = float4(0.0, 0.0, totalSum * params.xy);

	for (i = 0; i < HISTOGRAM_BINS; i++)
		FilterLuminance(buffer, i, maxHistogramValue, scaleOffset, filter);

	// Clamp to user brightness range
	return clamp(filter.x / max(filter.y, 1e-3f), params.z, params.w);
}
