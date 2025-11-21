#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float> src_img,
	Image<float> dst_img,
	float max_lum,
	uint2 blur_offset,
	uint blur_radius) {
	int2 id = dispatch_id().xy;
	auto src = src_img.read(id).xyz;
	if (all(src.xyz < max_lum)) {
		return 0;
	}
	int v = -(int)blur_radius;
	float4 result;
	for (int i = -(int)blur_radius; i <= blur_radius; ++i) {
		if (i == 0) continue;
		int2 offset_id = id + i * int2(blur_offset);
		if (any(offset_id < 0 || offset_id >= int2(dispatch_size().xy))) {
			continue;
		}
		float4 neighbor = src_img.read(offset_id);
		if (all(neighbor.xyz < max_lum)) {
			result += float4(neighbor.xyz, 1.f);
		}
	}
	if (result.w >= 1e-3f) {
		src = result.xyz / result.w;
	} else {
		float lum = max(src.x, max(src.y, src.z));
		src /= lum;
		lum = min(lum, max_lum + 1e-2f);
		src *= lum;
	}
    dst_img.write(id, float4(src, 1.0f));
	return 0;
}
