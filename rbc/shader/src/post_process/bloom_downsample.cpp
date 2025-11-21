#include "bloom.hpp"

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& out_tex,
	SampleImage& main_img) {
	auto uv = (float2(dispatch_id().xy) + 0.5f) / float2(dispatch_size().xy);
	out_tex.write(dispatch_id().xy, FragDownsample13(main_img, uv));
	return 0;
}