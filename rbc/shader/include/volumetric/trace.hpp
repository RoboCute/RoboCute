#pragma once

#include <volumetric/volume.hpp>
#include <path_tracer/trace.hpp>
#include <utils/onb.hpp>
#include <bsdfs/base/utils.hpp>
#include <bsdfs/base/bsdf_flags.hpp>

namespace mtl {

constexpr const uint32 MAX_VOLUME_STEPS = 16u;
constexpr const uint32 MIN_VOLUME_STEPS_BEFORE_RR = 3u;
constexpr const float SKY_DISTANCE = 32.0f;

int sample_channel(float3 albedo, float3 throughput, float rand, float3& channel_probs) {
	// Sample color channel in proportion to throughput
	channel_probs = throughput / max(DENOM_TOLERANCE, reduce_sum(throughput));
	float cdf = 0.0;
	for (int channel = 0; channel < 2; ++channel) {
		cdf += channel_probs[channel];
		if (rand < cdf)
			return channel;
	}
	return 2;
}

float3 sample_phase_function(float3 dir, float g, float2 rand) {
	if (abs(g) < 1e-4f)
		return uniform_sample_sphere(rand);
	float costheta = 1.0 / (2.0 * g) * (1.0 + g * g - sqr((1.0 - sqr(g)) * (1.0 - g + 2.0 * g * rand.x)));
	float sintheta = sqrt(max(0.0, 1.0 - costheta * costheta));
	float phi = 2.0 * pi * rand.y;
	mtl::Onb basis(dir);
	return costheta * basis.normal + sintheta * (cos(phi) * basis.tangent + sin(phi) * basis.bitangent);
}

CommittedHit trace_volumetric(auto const& volume_stack,
							 float3& throughput,
							 ShadingDetail& detail,
							 Ray& ray,
							 TraceIndices auto const& idxs,
							 auto& rng,
							 ProceduralGeometry& procedural_geometry,
							 uint mask = max_uint32,
							 uint2 steps = uint2(MAX_VOLUME_STEPS, MIN_VOLUME_STEPS_BEFORE_RR)) {
	bool trace = false;
	Volume volume;
	volume.extinction = float3(0.0f);
	if (!volume_stack.empty()) {
		volume = volume_stack.back();
		trace = any(volume.albedo != float3(0.0f));
	}
	if (!trace) {
		auto result = rbq_trace_closest(ray, idxs, rng, procedural_geometry, mask);
		throughput *= exp(-volume.extinction * (result.hit_triangle() ? result.ray_t : SKY_DISTANCE));
		return result;
	}
	// Do an "analogue random-walk" in the scattering medium, i.e. following the physical path of a photon.
	// Returns whether a surface hit occurred (and the hit data), and the volumetric path throughput.
	float3 mfp = 1.0f / max(float3(DENOM_TOLERANCE), volume.extinction);
	for (int n = 0; n < steps.x; ++n) {
		float3 channel_probs;
		int channel = sample_channel(volume.albedo, throughput, rng.next(g_buffer_heap), channel_probs);
		ray.t_max = -log(rng.next(g_buffer_heap)) * mfp[channel];
		CommittedHit surface_hit = rbq_trace_closest(ray, idxs, rng, procedural_geometry, mask);
		if (surface_hit.hit_triangle()) {
			if (n > 0)
				detail = max(detail, ShadingDetail::IndirectDiffuse);
			// ray hits surface within walk_step, walk terminates.
			// update walk throughput on exit (via MIS)
			float3 transmittance = exp(-surface_hit.ray_t * volume.extinction);
			throughput *= transmittance / max(DENOM_TOLERANCE, dot(channel_probs, transmittance));
			return surface_hit;
		}
		// Scatter within the medium, and continue walking.
		// First, make a Russian-roulette termination decision (after a minimum number of steps has been taken)
		float termination_prob = 0.0f;
		if (n > steps.y) {
			float continuation_prob = clamp(reduce_max(throughput), 0.0f, 1.0f);
			float termination_prob = 1.0 - continuation_prob;
			if (rng.next(g_buffer_heap) < termination_prob)
				break;
			throughput /= continuation_prob;// update walk throughput due to RR continuation
		}

		// update walk throughput on scattering in medium (via MIS)
		float3 transmittance = exp(-ray.t_max * volume.extinction);
		throughput *= volume.albedo * volume.extinction * transmittance;
		throughput /= max(DENOM_TOLERANCE, dot(channel_probs, volume.extinction * transmittance));

		// walk in the sampled direction, staying inside the medium
		ray.set_origin(ray.origin() + ray.t_max * ray.dir());

		// scatter into a new direction sampled from Henyey-Greenstein phase function
		ray.set_dir(normalize(sample_phase_function(ray.dir(), volume.anisotropy, rng.next2f(g_buffer_heap))));
	}
	// path terminated in the medium
	// throughput = float3(0.0);
	CommittedHit empty;
	empty.inst = max_uint32;
	empty.ray_t = 1e30f;
	empty.hit_type = HitTypes::Miss;
	return empty;
}
}// namespace mtl