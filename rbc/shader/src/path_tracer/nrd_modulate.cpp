#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>
#include <sampling/heitz_sobol.hpp>
#include <sampling/pcg.hpp>
using namespace luisa::shader;
float3 _NRD_YCoCgToLinear(float3 color) {
	float t = color.x - color.z;
	float3 r;
	r.y = color.x + color.z;
	r.x = t + color.y;
	r.z = t - color.y;
	return max(r, float3(0.0f));
}

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& specular_img,
	Image<float>& diffuse_img,
	Image<float>& spec_albedo_img,
	Image<float>& albedo_img,
	Image<float>& emission_img,
	bool denoise) {
	auto coord = dispatch_id().xy;
	auto albedo = albedo_img.read(coord);
	auto spec_albedo = max(spec_albedo_img.read(coord).xyz, float3(1e-3f));

	auto emission = emission_img.read(coord).xyz;
	if (albedo.w < 1e-10) {
		emission_img.write(
			coord,
			float4(emission, 1.f));
		return 0;
	}
	// auto di_col = _NRD_YCoCgToLinear(di_img.read(coord).xyz);// rgb
	float3 col;
	if (denoise) {
		col = specular_img.read(coord).xyz * spec_albedo +
			  _NRD_YCoCgToLinear(diffuse_img.read(coord).xyz) * max(albedo.xyz, float3(1e-3f));
	} else {
		col = specular_img.read(coord).xyz +
			  _NRD_YCoCgToLinear(diffuse_img.read(coord).xyz);
	}
	col += emission;
	col = spectrum::spectrum_to_tristimulus(col);
	emission_img.write(
		coord,
		float4(col, 1.f));
	// float4(di_img.read(coord).xyz, 1.f));

	return 0;
}