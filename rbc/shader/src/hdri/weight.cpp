#include <luisa/std.hpp>
#include <spectrum/color_space.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	SampleImage& img,
	Buffer<float>& scale_map) {
	auto pixel = dispatch_id().xy;
	auto center = float2(pixel) + .5f;
	auto sum_weight = 0.f;
	auto sum_scale = 0.f;
	const auto filter_radius = 1.f;
	const auto filter_step = .125f;
	const auto vv = filter_radius / filter_step;
	auto n = (int)ceil(vv + 1e-5f);
	float3x3 identity(
		1, 0, 0,
		0, 1, 0,
		0, 0, 1);
	for (int dy = -n; dy < n + 1; ++dy) {
		for (int dx = -n; dx < n + 1; ++dx) {
			auto offset = float2(int2(dx, dy)) * filter_step;
			auto uv = (center + offset) / float2(dispatch_size().xy);
			float theta;
			float phi;
			float3 dir;
			sampling::sphere_uv_to_direction_theta(identity, uv, theta, phi, dir);
			auto col = img.sample(uv, Filter::POINT, Address::EDGE);
			float scale = spectrum::srgb_to_illuminance(col.xyz);
			auto sin_theta = sin(uv.y * pi);
			auto weight = exp(-4.f * length_squared(offset));
			auto value = weight * min(scale * sin_theta, 1e8f);
			sum_weight += weight;
			sum_scale += value;
		}
	}
	auto pixel_id = pixel.y * dispatch_size().x + pixel.x;
	scale_map.write(pixel_id, sum_scale / sum_weight);
	return 0;
}