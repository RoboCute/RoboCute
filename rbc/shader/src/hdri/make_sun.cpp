#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float> src_img,
	float3 sun_color,
	float3 sun_dir,
	float sun_angle_radians) {
	auto id = dispatch_id().xy;
	auto size = dispatch_size().xy;
	auto uv = (float2(id) + 0.5f) / float2(size);
	float3x3 identity(
		1, 0, 0,
		0, 1, 0,
		0, 0, 1);
	auto sky_dir = sampling::sphere_uv_to_direction(identity, uv);
	auto angle = acos(dot(normalize(sky_dir), -sun_dir));
	if (angle <= sun_angle_radians) {
        float4 col = src_img.read(id);
        col.xyz += sun_color;
        src_img.write(id, col);
	}
    return 0;
}