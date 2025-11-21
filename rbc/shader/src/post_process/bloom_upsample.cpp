#include "bloom.hpp"

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& out_tex,
	SampleImage& main_img,
	SampleImage& bloom_img,
	float sample_scale) {
	auto uv = (float2(dispatch_id().xy) + 0.5f) / float2(dispatch_size().xy);
	out_tex.write(dispatch_id().xy, FragUpsampleTent(main_img, bloom_img, uv, sample_scale));
	return 0;
}