#include <luisa/std.hpp>
#include <spectrum/color_space.hpp>
#include <spectrum/spectrum.hpp>
using namespace luisa::shader;
[[kernel_2d(16, 16)]] int kernel(
	Image<float> src_img,
	Image<float> dst_img) {
	float3 col = src_img.read(dispatch_id().xy).xyz;
	dst_img.write(dispatch_id().xy, spectrum::srgb_to_illuminance(col));
	return 0;
}