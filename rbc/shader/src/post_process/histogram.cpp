#include <post_process/histogram.hpp>
[[kernel_2d(16, 16)]] int kernel(
	Buffer<uint>& histogram_buffer,
	SampleImage& source_img,
	float4 scale_offset_res) {
	auto groupThreadId = thread_id().xy;
	auto dispatchThreadId = dispatch_id().xy;
	SharedArray<uint, HISTOGRAM_BINS> gs_histogram;
	uint localThreadId = groupThreadId.y * HISTOGRAM_THREAD_X + groupThreadId.x;
	if (localThreadId < HISTOGRAM_BINS) {
		gs_histogram[localThreadId] = 0u;
	}
	float2 ipos = float2(dispatchThreadId) * 2.0f;
	sync_block();
	if (all(ipos < scale_offset_res.zw)) {
		uint weight = 1u;
		float2 sspos = ipos / scale_offset_res.zw;

		// Vignette weighting to put more focus on what's in the center of the screen
		// {
		// 	float2 d = abs(sspos - float2(0.5));
		// 	float vfactor = saturate(1.0 - dot(d, d));
		// 	vfactor *= vfactor;
		// 	weight = (uint)(64.0 * vfactor);
		//     if (weight < 1) {
		//         weight = 1;
		//     }
		// }

		float3 color = source_img.sample(sspos, Filter::LINEAR_POINT, Address::EDGE).xyz;
		float luminance = rec2020_Luminance(color);
		float logLuminance = GetHistogramBinFromLuminance(luminance, scale_offset_res.xy);
		uint idx = (uint)(logLuminance * (HISTOGRAM_BINS - 1u));
		gs_histogram.atomic_fetch_add(idx, weight);
	}
	sync_block();
	if (localThreadId < HISTOGRAM_BINS) {
		histogram_buffer.atomic_fetch_add(localThreadId, gs_histogram[localThreadId]);
	}
	return 0;
}