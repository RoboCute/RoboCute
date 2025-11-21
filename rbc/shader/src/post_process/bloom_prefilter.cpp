#include "bloom.hpp"

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& out_tex,
	SampleImage& main_img,
	float params,
	float4 threshold,
	Buffer<float>& exposure) {
	auto uv = (float2(dispatch_id().xy) + 0.5f) / float2(dispatch_size().xy);
	out_tex.write(dispatch_id().xy, FragPrefilter13(main_img, exposure.read(0), uv, params, threshold));
	return 0;
}