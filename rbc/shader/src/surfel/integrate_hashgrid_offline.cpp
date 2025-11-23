#include <surfel/surfel_grid.hpp>
#include <spectrum/spectrum.hpp>
#include <utils/onb.hpp>
#include <sampling/sample_funcs.hpp>
#include <path_tracer/gbuffer.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<GBuffer>& gbuffers,
	Image<float>& emission_img,
	Buffer<uint>& value_buffer,
	Image<uint>& surfel_mark,
	uint max_accum,
	float emission_rate) {
	auto id = dispatch_id().xy;
	auto gbuffer = gbuffers.read(id.x + id.y * dispatch_size().x);
	auto key_slot = surfel_mark.read(id).x;
	float3 origin_radiance(gbuffer.radiance[0], gbuffer.radiance[1], gbuffer.radiance[2]);
	// borrow radiance as reject mask
	// see pt_multi_bounce.cpp
	bool reject = any(origin_radiance < -1.f);
	origin_radiance = max(origin_radiance, float3(0.f));
	if (key_slot != max_uint32 && !reject) {
		auto slot = key_slot * OFFLINE_SURFEL_INT_SIZE;
		float3 radiance = float3(
			bit_cast<float>(value_buffer.read(slot)),
			bit_cast<float>(value_buffer.read(slot + 1)),
			bit_cast<float>(value_buffer.read(slot + 2)));
		radiance /= min(float(max_accum), float(value_buffer.read(slot + 3)));
		origin_radiance = lerp(radiance, origin_radiance.xyz, 1.0f / float(max_accum));// TODO: difference with material
	}
	auto beta = float3(gbuffer.beta[0], gbuffer.beta[1], gbuffer.beta[2]);
	if (any(beta < -1e-5f)) {
		beta = float3(1);
	}
	auto emission = emission_img.read(id);
	if (reject) {
		emission.w -= 1;
		emission.w = max(emission.w, 0.f);
		beta = float3(0.f);
	}
	origin_radiance = float3(origin_radiance.xyz * beta + emission.xyz);
	emission_img.write(id, float4(origin_radiance * emission_rate, emission.w));
	return 0;
}