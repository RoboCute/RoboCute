#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <sampling/pcg.hpp>
#include <spectrum/spectrum.hpp>
#include <path_tracer/modulate_result_common.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& beta_img,
	Image<float>& emission_tex,
	Image<float>& radiance_img,
	Image<float>& di_img,
	Image<float>& out_radiance_img,
	Buffer<float>& exposure_buffer,
	uint sample,
	uint spp) {
	auto id = dispatch_id().xy;
	bool init_sample = sample == 0;
	float4 radiance = radiance_img.read(id);
	auto beta = beta_img.read(id).xyz;
	beta *= beta;
	radiance.xyz *= beta;
	radiance = clamp(radiance, 1e-5f, 65500.0f);
	radiance.xyz = realtime_clamp_radiance(radiance.xyz, exposure_buffer);
	float rate = 1.0f / spp;
	radiance *= rate;
	if (!init_sample) {
		radiance += out_radiance_img.read(id);
	}

	if (sample == spp - 1) {
		radiance.xyz += di_img.read(id).xyz;
		radiance.xyz += emission_tex.read(id).xyz;
	}
	out_radiance_img.write(id, radiance);
	return 0;
}
