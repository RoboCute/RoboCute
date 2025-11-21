#pragma once

#include <restir/restirCommon.hpp>

using namespace luisa::shader;
// struct Reservoir
#include <restir/restir_types.inl>
#include <lighting/importance_sampling.hpp>
template<typename RandomSampler>
static void RestirInit(
	Accel& accelStructure,
	float3 worldPos,
#ifdef ENABLE_RESERVOIR_TEMPORAL
	float3 motionVector,
#endif
	float3 plane_normal,
	float3 normal,
	float3 reflDir,
	float roughness,
	uint matId,
#ifdef ENABLE_RESERVOIR_TEMPORAL
	Image<uint>& lightDirtyMap,
	bool resetFrame,
	Image<uint>& ReservoirPrev,
#endif
	Image<uint>& ReservoirCurr,
	// sobol
	BindlessArray& heap,
	RandomSampler& lcg_sampler,
	//camera
	float3 cam_pos,
	float4x4 inverseViewProjMatrix,
#ifdef ENABLE_RESERVOIR_TEMPORAL
	float4x4 lastViewProjMatrix,
#endif
	//light
	uint lightsCount,
	uint meshLightsHeapIdx,
	uint bvhHeapIdx,
	uint pointLightsHeapIdx,
	uint spotLightsHeapIdx,
	uint areaLightsHeapIdx,
	uint instBufferHeapIdx,
	uint matIdxBufferHeapIdx,
	// sky
	float3x3 colorspace_matrix,
	float3x3 world_2_sky_mat,
	uint sky_lum_idx,
	uint sky_alias_heap_idx,
	uint sky_pdf_heap_idx) {
	uint2 launchIndex = dispatch_id().xy;
	uint2 launchDim = dispatch_size().xy;

	float3 viewDir = cam_pos - worldPos;
	float viewLen = max(1e-5f, length(viewDir));
	viewDir /= viewLen;
	Reservoir reservoir;
	uint tlasIndex = max_uint32;
	if (matId == 0 || matId == 2) {
		roughness = 1.0f;
	}
	if (roughness >= DI_ROUGHNESS_THRESHOLD) {
		uint lightToSample;
		float2 lightSampledUV;
		float init_lightPdf, lightPdf;
		float distToLight;
		float3 init_lightIntensity, lightIntensity;
		float3 init_toLight, toLight;
#ifdef ENABLE_RESERVOIR_TEMPORAL
		uint2 prevBufferIdx;
		bool enableTemporalResue = false;
		// resetFrame = true;
		if (!resetFrame) {
			float3 prevWorldPos = worldPos - motionVector;
			float4 screen_space = lastViewProjMatrix * float4(prevWorldPos, 1.0f);
			screen_space /= screen_space.w;
			screen_space.xy = screen_space.xy * 0.5f + 0.5f;
			int2 originPrevIndex;
			originPrevIndex.x = screen_space.x * (float)launchDim.x;
			originPrevIndex.y = screen_space.y * (float)launchDim.y;
			// minimum diff 0.01 * viewLen
			float minToPrevDist = viewLen * 0.01f;
			for (int x = -1; x <= 1; ++x)
				for (int y = -1; y <= 1; ++y) {
					auto prevIndex = originPrevIndex + int2(x, y);
					if (!(all(prevIndex < int2(launchDim) && prevIndex >= 0))) continue;
					float4 prevNormRough = gbufferNormalRoughness.read(prevIndex);
					float3 prevNormal = sampling::decode_unit_vector(normRough.xy);
					if (dot(prevNormal, normal) > 0.5) {
						float2 screen_uv = (float2(prevIndex) + 0.5f) / float2(launchDim);
						float prevDepth = gbufferDepth.read(prevIndex).x;
						float4 prevDepthWorldPos = inverseViewProjMatrix * float4(screen_uv * 2.0f - 1.0f, prevDepth, 1);
						prevDepthWorldPos /= prevDepthWorldPos.w;
						float toPrevDist = distance(prevDepthWorldPos.xyz, prevWorldPos);
						if (toPrevDist < minToPrevDist) {
							minToPrevDist = toPrevDist;
							prevBufferIdx = prevIndex;
							enableTemporalResue = true;
						}
					}
				}
		}
#endif
		reservoir.lightSampledIndex = max_uint32;
		float p_hat;
		// Sample local light
		float result_p_hat = -1.0f;
		// lightsCount = 0;
		if (lightsCount > 0) {
			// choose light
			uint light_type;
			uint light_idx;
			uint tlas_idx;
			float light_probability = 1.0f;
			lighting::MeshLight mesh_light;
			bool selectResult;
			float3 di_normal = normal;
			float3 di_plane_normal = plane_normal;
			if (matId == 0 || matId == 2) {
				if (matId == 2) {
					di_normal = -di_normal;
					di_plane_normal = -di_plane_normal;
				}
				selectResult = lighting::select_light(
					heap,
					lcg_sampler,
					[&](
						lighting::Bounding bound,
						float4 cone) -> float {
					float3 center = (bound.min + bound.max) * 0.5f;
					float3 box_size = bound.size();
					float box_dist = length(box_size);
					float to_bounding_dist = max(sampling::sd_box(worldPos - center, box_size * 0.5f), 0.02f);
					float3 point_to_light = center + plane_normal * box_dist * 0.5f - worldPos;
					float in_sphere = 1.0f - saturate(dot(point_to_light, plane_normal));
					in_sphere *= in_sphere;
					in_sphere = 1.0f - in_sphere;

					auto point_dist = length(point_to_light);
					point_to_light /= point_dist;
					auto angle = dot(-point_to_light, cone.xyz);
					angle = min(cone.w / max(angle, 1e-4f), 1.f);
					auto angle_importance = 16384.0f * select(angle * angle, 1.f, cone.w > pi * 0.99f);
					return angle_importance * in_sphere / to_bounding_dist;
				},
					meshLightsHeapIdx,
					bvhHeapIdx,
					light_type,
					light_idx,
					tlas_idx,
					light_probability,
					mesh_light);
			} else if (matId == 1 || matId == 3) {
				if (matId == 3) {
					di_normal = -di_normal;
					di_plane_normal = -di_plane_normal;
				}
				selectResult = lighting::select_light(
					heap,
					lcg_sampler,
					[&](
						lighting::Bounding bound,
						float4 cone) -> float {
					float3 center = (bound.min + bound.max) * 0.5f;
					float3 box_size = bound.size();
					float box_dist = length(box_size);
					float to_bounding_dist = max(sampling::sd_box(worldPos - center, box_size * 0.5f), 0.02f);
					float3 point_to_light = center + plane_normal * box_dist * 0.5f - worldPos;
					float in_sphere = 1.0f - saturate(dot(point_to_light, plane_normal));
					in_sphere *= in_sphere;
					in_sphere = 1.0f - in_sphere;

					auto point_dist = length(point_to_light);
					point_to_light /= point_dist;
					auto angle = dot(-point_to_light, cone.xyz);
					angle = min(cone.w / max(angle, 1e-4f), 1.f);
					auto angle_importance = select(angle * angle, 1.f, cone.w > pi * 0.99f);

					auto bary_importance = 16384.0f / max(0.02f, point_dist);
					bary_importance = pow(saturate(dot(reflDir, point_to_light)), 1.f / clamp(roughness, 0.4f, 0.9f));

					return angle_importance * bary_importance * in_sphere / to_bounding_dist;
				},
					meshLightsHeapIdx,
					bvhHeapIdx,
					light_type,
					light_idx,
					tlas_idx,
					light_probability,
					mesh_light);
			}
			if (selectResult) {
				lightToSample = (light_type << 29u) | (light_idx);
				lightSampledUV = lcg_sampler.next2f();
				// lightToSample = min(int(randSampler.next(heap) * lightsCount), lightsCount - 1);
				float light_misWeight;
				getLightData(heap, colorspace_matrix, world_2_sky_mat, sky_lum_idx, sky_pdf_heap_idx, pointLightsHeapIdx, spotLightsHeapIdx, areaLightsHeapIdx, meshLightsHeapIdx, instBufferHeapIdx, matIdxBufferHeapIdx, lightToSample, tlas_idx, lightSampledUV, worldPos, toLight, lightIntensity, distToLight, lightPdf, light_misWeight);
				lightPdf *= ite(dot(toLight, plane_normal) < 1e-5f, 0.f, 1.0f);
				if (lightPdf > 1e-5f) {
					auto pdf = computePdf(viewDir, normal, toLight, roughness, matId);
					pdf *= inv_pi;
					reservoir.M = 1;
					Ray shadow_ray(worldPos, toLight, 1e-3f, max(1e-5f, distToLight - 1e-3f));
					auto mis = sampling::weighted_balanced_heuristic(light_misWeight, lightPdf, pdf);
					p_hat = CalcLum(lightIntensity) * pdf / lightPdf * mis;
					if (accelStructure.trace_any(shadow_ray, ONLY_OPAQUE_MASK)) {
						result_p_hat = -1.0f;
						init_lightPdf = 0;
						init_lightIntensity = float3(0);
						init_toLight = float3(0);
					} else {
						reservoir.initReservoir(lightToSample, p_hat / light_probability, lightSampledUV);
						tlasIndex = tlas_idx;
						result_p_hat = p_hat;
#ifdef ENABLE_RESERVOIR_TEMPORAL
						init_lightPdf = lightPdf;
						init_lightIntensity = lightIntensity;
						init_toLight = toLight;
#endif
					}
				}
			}
		}

		// initialize previous reservoir if this is the first iteraation
		//to choose an arbitrary light for this pixel after choosing 32 random light candidates
		if (sky_lum_idx != max_uint32) {
			uint buffer_sample_id;
			sample_hdri_alias_table(heap, sky_lum_idx, sky_alias_heap_idx, lcg_sampler.next2f(), lightSampledUV, buffer_sample_id);
			float skyLum;
			getSkyLum(heap, world_2_sky_mat, sky_lum_idx, sky_pdf_heap_idx, lightSampledUV, buffer_sample_id, toLight, skyLum, lightPdf);
			Ray shadow_ray(worldPos, toLight, 1e-3f, 1e30f);
			float dotV = dot(toLight, plane_normal);
			if (matId == 2 || matId == 3) {
				dotV = -dotV;
			}
			if (dotV > 1e-5f) {
				reservoir.M = 1;
				if (!accelStructure.trace_any(shadow_ray, ONLY_OPAQUE_MASK)) {
					auto pdf = computePdf(viewDir, normal, toLight, roughness, matId);
					pdf *= inv_pi;
					auto mis = sampling::balanced_heuristic(lightPdf, pdf);
					p_hat = skyLum * pdf / max(1e-5f, lightPdf) * mis;
					if (reservoir.updateReservoir(max_uint32, p_hat, lcg_sampler.next(), lightSampledUV)) {
						result_p_hat = p_hat;
						tlasIndex = max_uint32;
#ifdef ENABLE_RESERVOIR_TEMPORAL
						init_lightPdf = lightPdf;
						init_lightIntensity = lightIntensity;
						init_toLight = toLight;
#endif
					}
				}
			}
		}
		p_hat = result_p_hat;
		if (p_hat > 0.f) {
			reservoir.targetPdf = reservoir.weightSum / p_hat;
			// reservoir.targetPdf = reservoir.weightSum / p_hat;
		} else {
			reservoir.targetPdf = 0.f;
		}
#ifdef ENABLE_RESERVOIR_TEMPORAL
		PackedReservoir prev_reservoir;
		if (enableTemporalResue) {
			prev_reservoir.Set(ReservoirPrev.read(prevBufferIdx));
			if (prev_reservoir.M > 0 && prev_reservoir.targetPdf > 1e-5f && !lighting::is_light_dirty(lightDirtyMap, prev_reservoir.lightSampledIndex)) {
				float3 prev_toLight;
				float3 prev_lightIntensity;
				float prev_lightPdf;

				reservoir.RefreshUpdate(p_hat);
				getLightData(heap, colorspace_matrix, world_2_sky_mat, sky_lum_idx, sky_alias_heap_idx, pointLightsHeapIdx, spotLightsHeapIdx, areaLightsHeapIdx, meshLightsHeapIdx, instBufferHeapIdx, matIdxBufferHeapIdx, prev_reservoir.lightSampledIndex, tlas_idx, prev_reservoir.GetLightUV(), worldPos, prev_toLight, prev_lightIntensity, distToLight, prev_lightPdf);
				if (prev_lightPdf > 1e-5f) {
					auto pdf = computePdf(viewDir, normal, prev_toLight, roughness, matId);
					pdf *= inv_pi;
					auto mis = sampling::balanced_heuristic(prev_lightPdf, pdf);
					p_hat = CalcLum(prev_lightIntensity) * pdf / max(1e-5f, prev_lightPdf) * mis;
					prev_reservoir.M = uint(min(lerp(20.0f, 1.0f, saturate(length(motionVector) / max(viewLen * 0.1f, 1e-5f))), float(prev_reservoir.M)) + 0.5f);
					if (reservoir.updateReservoir(prev_reservoir.lightSampledIndex, p_hat * prev_reservoir.targetPdf * prev_reservoir.M, randSampler.next(heap), prev_reservoir.lightSampledUV)) {
						lightIntensity = prev_lightIntensity;
						toLight = prev_toLight;
						lightPdf = prev_lightPdf;
					} else {
						lightPdf = init_lightPdf;
						lightIntensity = init_lightIntensity;
						toLight = init_toLight;
					}
					reservoir.M += prev_reservoir.M;
					if (lightPdf > 1e-5f) {
						auto pdf = computePdf(viewDir, normal, toLight, roughness, matId);
						pdf *= inv_pi;
						auto mis = sampling::balanced_heuristic(lightPdf, pdf);
						p_hat = CalcLum(lightIntensity) * pdf / max(1e-5f, lightPdf) * mis;
						reservoir.targetPdf = (1.f / max(p_hat, 0.0001f)) * (reservoir.weightSum / max(reservoir.M, 1));
					}
				}
			}
			// }
		}
#endif
	}
	// reservoir.clampReservoir();
	PackedReservoir result;

	if (reservoir.M == 0) {
		result.targetPdf = -1.0f;
	} else {
		result.targetPdf = reservoir.targetPdf;
	}
	result.lightSampledIndex = reservoir.lightSampledIndex;
	result.lightSampledUV = reservoir.lightSampledUV;
	result.tlasIndex = tlasIndex;
	ReservoirCurr.write(launchIndex, result.Get());
}