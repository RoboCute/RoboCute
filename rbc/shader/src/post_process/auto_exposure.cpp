// #define DEBUG
#include <luisa/printer.hpp>
#include <post_process/histogram.hpp>

float GetExposureMultiplier(float avgLuminance, float4 _Params2) {
	avgLuminance = max(1e-3f, avgLuminance);
	//float keyValue = 1.03 - (2.0 / (2.0 + log2(avgLuminance + 1.0)));
	float keyValue = _Params2.z;
	float exposure = keyValue / avgLuminance;
	return exposure;
}

float InterpolateExposure(float newExposure, float oldExposure, float4 _Params2) {
	float delta = newExposure - oldExposure;
	float speed = delta > 0.0 ? _Params2.x : _Params2.y;
	float exposure = oldExposure + delta * (1.0 - exp2(-_Params2.w * speed));
	return exposure;
}
[[kernel_2d(16, 8)]] int kernel(
	float global_exposure,
	float4 _Params1,	   // x: lowPercent, y: highPercent, z: minBrightness, w: maxBrightness
	float4 _Params2,	   // x: speed down, y: speed up, z: exposure compensation, w: delta time
	float4 _ScaleOffsetRes,// x: scale, y: offset, w: histogram pass width, h: histogram pass height
	bool progressive,
	Buffer<uint>& _HistogramBuffer,
	Buffer<float>& SrcDst) {
	SharedArray<uint, HISTOGRAM_REDUCTION_BINS> gs_pyramid;
	auto groupThreadId = thread_id().xy;
	uint thread_id = groupThreadId.y * HISTOGRAM_REDUCTION_THREAD_X + groupThreadId.x;
	gs_pyramid[thread_id] = _HistogramBuffer.read(thread_id);
	sync_block();
	for (uint i = (HISTOGRAM_REDUCTION_BINS) >> 1u; i > 0u; i >>= 1u) {
		if (thread_id < i) {
			if (gs_pyramid[thread_id] < gs_pyramid[thread_id + i]) {
				gs_pyramid[thread_id] = gs_pyramid[thread_id + i];
			}
			// gs_pyramid[thread_id] = max(gs_pyramid[thread_id], gs_pyramid[thread_id + i]);
		}
		sync_block();
	}
	if (thread_id == 0u) {
		float maxValue = 1.0 / float(gs_pyramid[0]);
		float exposure;
		float avgLuminance = GetAverageLuminance(_HistogramBuffer, _Params1, maxValue, _ScaleOffsetRes.xy);
		if (global_exposure < 0) {
			exposure = GetExposureMultiplier(avgLuminance, _Params2);
			if (progressive) {
				float prevExposure = SrcDst.read(1);
				exposure = InterpolateExposure(exposure, prevExposure, _Params2);
			}
		} else {
			exposure = global_exposure;
		}
		SrcDst.write(0, exposure);
		SrcDst.write(1, exposure);
		SrcDst.write(2, avgLuminance);
		SrcDst.write(3, log2(avgLuminance));
	}
	return 0;
}
