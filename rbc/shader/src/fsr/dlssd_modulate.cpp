#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>
using namespace luisa::shader;
float3 _NRD_YCoCgToLinear(float3 color) {
	float t = color.x - color.z;
	float3 r;
	r.y = color.x + color.z;
	r.x = t + color.y;
	r.z = t - color.y;

	return max(r, float3(0.0f));
}
float3 Decode(float4 data) {
	// return _NRD_YCoCgToLinear(data.xyz);
	return data.xyz;
}

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& albedo_img,
	Image<float>& emission_img,
	Image<float>& radiance_img) {
	auto coord = dispatch_id().xy;
	auto albedo = albedo_img.read(coord);
	auto emission = emission_img.read(coord).xyz;
	if (albedo.w < 1e-10) {
		radiance_img.write(
			coord,
			float4(emission, 1.f));
		return 0;
	}
	auto radiance = radiance_img.read(coord).xyz;
	auto col = spectrum::spectrum_to_tristimulus(emission + radiance);

	radiance_img.write(
		coord,
		float4(col, 1.f));
	return 0;
}