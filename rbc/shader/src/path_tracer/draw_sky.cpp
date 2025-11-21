#include <luisa/std.hpp>
#include <luisa/resources/texture.hpp>
#include <sampling/sample_funcs.hpp>
using namespace luisa::shader;

// Normal encoding variants ( match NormalEncoding )
#define NRD_NORMAL_ENCODING_RGBA8_UNORM 0
#define NRD_NORMAL_ENCODING_RGBA8_SNORM 1
#define NRD_NORMAL_ENCODING_R10G10B10A2_UNORM 2// supports material ID bits
#define NRD_NORMAL_ENCODING_RGBA16_UNORM 3
#define NRD_NORMAL_ENCODING_RGBA16_SNORM 4// also can be used with FP formats

// Roughness encoding variants ( match RoughnessEncoding )
#define NRD_ROUGHNESS_ENCODING_SQ_LINEAR 0	// linearRoughness * linearRoughness
#define NRD_ROUGHNESS_ENCODING_LINEAR 1		// linearRoughness
#define NRD_ROUGHNESS_ENCODING_SQRT_LINEAR 2// sqrt( linearRoughness )

#define NRD_NORMAL_ENCODING NRD_NORMAL_ENCODING_R10G10B10A2_UNORM
#define NRD_ROUGHNESS_ENCODING NRD_ROUGHNESS_ENCODING_LINEAR

float2 _NRD_EncodeUnitVector(float3 v, const bool bSigned) {
	v /= dot(abs(v), float3(1));

	float2 octWrap = (float2(1.0f) - abs(v.yx)) * (ite(v.xy >= 0.0f, float2(1.f), float2(0.f)) * 2.0f - 1.0f);
	v.xy = ite(v.z >= 0.0f, v.xy, octWrap);

	return ite(bSigned, v.xy, v.xy * 0.5f + 0.5f);
}

float4 NRD_FrontEnd_PackNormalAndRoughness(float3 N, float roughness, float materialID) {
	float4 p;

#if (NRD_ROUGHNESS_ENCODING == NRD_ROUGHNESS_ENCODING_SQRT_LINEAR)
	roughness = sqrt(saturate(roughness));
#elif (NRD_ROUGHNESS_ENCODING == NRD_ROUGHNESS_ENCODING_SQ_LINEAR)
	roughness *= roughness;
#endif

#if (NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_R10G10B10A2_UNORM)
	p.xy = _NRD_EncodeUnitVector(N, false);
	p.z = roughness;
	p.w = materialID;
#else
	// Best fit ( optional )
	N /= max(abs(N.x), max(abs(N.y), abs(N.z)));

#if (NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_RGBA8_UNORM || NRD_NORMAL_ENCODING == NRD_NORMAL_ENCODING_RGBA16_UNORM)
	N = N * 0.5f + 0.5f;
#endif

	p.xyz = N;
	p.w = roughness;
#endif

	return p;
}

[[kernel_2d(16, 8)]] int kernel(
	Image<float> _emission,
	Image<float> _depth,
	Image<float> _normal_roughness,
	Image<float> _mv,
	Image<float> _spec_albedo,
	Image<float> _albedo,
	Image<float> _confidence,
#ifdef USE_DLSS
	Image<float> _diffuse,
#else
	Image<float> _out_radiance,
	Image<float> _specular_dist,
	Image<float> _specular_albedo,
	Image<float> _reflection_mv,
#endif
	BindlessImage& heap,
	uint sky_idx,
	float default_depth,
	float3x3 resource_to_rec2020_mat,
	float3x3 world_2_sky_mat,
	float4x4 inv_vp,
	float3 cam_pos,
	float2 jitter) {
	auto coord = dispatch_id().xy;
	auto size = dispatch_size().xy;
	auto uv = (float2(coord) + jitter + float2(0.5)) / float2(size);
	auto proj = float4((uv * 2.f - 1.0f), 0.f, 1);
	auto world_pos = inv_vp * proj;
	world_pos /= world_pos.w;
	auto dir = normalize(world_pos.xyz - cam_pos);
	float4 col(0);
	if (sky_idx != max_uint32) {
		col = heap.uniform_idx_image_sample(sky_idx, sampling::sphere_direction_to_uv(world_2_sky_mat, dir), Filter::LINEAR_POINT, Address::EDGE);
	}
	col.xyz = resource_to_rec2020_mat * col.xyz;
	_emission.write(coord, col);
	// if (sky_idx != max_uint32) {
	// 	col = heap.uniform_idx_image_sample(sky_idx, sampling::sphere_direction_to_uv(dir));
	// }
	_depth.write(coord, float4(default_depth));
	_normal_roughness.write(coord, NRD_FrontEnd_PackNormalAndRoughness(float3(0, 0, 1), 0, 1.0));
	_mv.write(coord, float4(0));
	_albedo.write(coord, float4(0));
	_spec_albedo.write(coord, float4(0));
	_confidence.write(coord, float4(0));
#ifdef USE_DLSS
	_diffuse.write(coord, float4(0));
#else
	_out_radiance.write(coord, float4(0));
	_specular_dist.write(coord, float4(0));
	_specular_albedo.write(coord, float4(0));
	_reflection_mv.write(coord, float4(0));
#endif
	return 0;
}