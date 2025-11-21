#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/modulate_result_common.hpp>
#include <spectrum/spectrum.hpp>
#include <bsdfs/base/bsdf_flags.hpp>
using namespace luisa::shader;

bool _NRD_IsInvalid(float3 x) {
	return !any(is_finite(x));
}

bool _NRD_IsInvalid(float x) {
	return !is_finite(x);
}
float3 _NRD_LinearToYCoCg(float3 color) {
	float Y = dot(color, float3(0.25, 0.5, 0.25));
	float Co = dot(color, float3(0.5, 0.0, -0.5));
	float Cg = dot(color, float3(-0.25, 0.5, -0.25));

	return float3(Y, Co, Cg);
}

#define NRD_FP16_MAX 65504.0
float4 REBLUR_FrontEnd_PackRadianceAndNormHitDist(float3 radiance, float normHitDist, bool sanitize) {
	if (sanitize) {
		radiance = _NRD_IsInvalid(radiance) ? float3(0, 0, 0) : clamp(radiance, 0, NRD_FP16_MAX);
		normHitDist = _NRD_IsInvalid(normHitDist) ? 0 : saturate(normHitDist);
	}

	radiance = _NRD_LinearToYCoCg(radiance);

	return float4(radiance, normHitDist);
}

[[kernel_2d(16, 8)]] int kernel(
	Image<float>& radiance_img,
	Image<float>& beta_img,
	Image<float>& spec_albedo_img,
	Image<float>& albedo_img,
	Image<float>& di_img,
	Image<uint>& sample_flags_img,
	Image<float>& normal_rough_img,
	Image<float>& diffuse_img,
	Image<float>& specular_img,
	Buffer<float>& exposure_buffer,
	uint sample,
	uint spp,
	bool denoise) {
	auto coord = dispatch_id().xy;
	auto sample_flags = mtl::BSDFFlags(sample_flags_img.read(coord).x);
	bool is_diffuse = mtl::is_diffuse(sample_flags);
	float4 radiance = radiance_img.read(coord);
	auto beta = beta_img.read(coord).xyz;
	beta *= beta;
	radiance.xyz *= beta;
	float rate = 1.0f / spp;
	radiance *= rate;
	if (sample == spp - 1) {
		radiance += di_img.read(coord);
		radiance = clamp(radiance, 1e-5f, 65500.0f);
		radiance.xyz = realtime_clamp_radiance(radiance.xyz, exposure_buffer);
	}
	auto diffuseResult = is_diffuse ? radiance : float4(0.f);
	auto specularResult = is_diffuse ? float4(0.f) : radiance;
	auto albedo = max(albedo_img.read(coord).xyz, float3(1e-3f));
	auto spec_albedo = max(spec_albedo_img.read(coord).xyz, float3(1e-3f));
	if (denoise) {
		diffuseResult.xyz /= albedo;
		specularResult.xyz /= spec_albedo;
	}
	// diffuseResult.xyz = spectrum::spectrum_to_tristimulus(diffuseResult.xyz);
	// specularResult.xyz = spectrum::spectrum_to_tristimulus(specularResult.xyz);
	diffuseResult = REBLUR_FrontEnd_PackRadianceAndNormHitDist(diffuseResult.xyz, diffuseResult.w, true);

	bool init_sample = sample == 0;

	if (init_sample) {
		diffuse_img.write(coord, diffuseResult);
		specular_img.write(coord, specularResult);
	} else {
		if (is_diffuse) {
			diffuse_img.write(coord, diffuseResult + diffuse_img.read(coord));
		} else {
			specular_img.write(coord, specularResult + specular_img.read(coord));
		}
	}
	return 0;
}