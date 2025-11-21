// #define DEBUG
#include <luisa/printer.hpp>
#include <luisa/std.hpp>
#include <post_process/lpm.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& img,
	Image<float>& dst_img,
	std::array<uint4, 10> ctl,
	bool shoulder,
	bool con,
	bool soft,
	bool con2,
	bool clip,
	bool scaleOnly,
	uint2 pad) {
	float4 color = img.read(dispatch_id().xy);
	LpmFilter(ctl, color.x, color.y, color.z, shoulder, con, soft, con2, clip, scaleOnly);
	dst_img.write(dispatch_id().xy, color);
	return 0;
}