#pragma once
#include <luisa/printer.hpp>
#include "types.hpp"
#include <geometry/vertices.hpp>
#include <sampling/pcg.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/sample_hdri.hpp>
#include <utils/onb.hpp>
#include <luisa/numeric.hpp>
#include <material/mats.hpp>
#include <path_tracer/trace.hpp>
#include <spectrum/spectrum.hpp>
#include <luisa/resources/common_extern.hpp>
#include <bsdfs/base/bsdf_flags.hpp>
// #include <material/DinseyBSDF.hpp>

#include <std/concepts>

namespace lighting {

template<class T>
concept BindlessIndices = requires(T t) {
	{ auto(t.time) } -> std::same_as<float>;
	{ auto(t.frame_countdown) } -> std::same_as<uint>;
	{ auto(t.light_count) } -> std::same_as<uint>;
	{ auto(t.alias_table_idx) } -> std::same_as<uint>;
	{ auto(t.pdf_table_idx) } -> std::same_as<uint>;
	{ auto(t.sky_heap_idx) } -> std::same_as<uint>;
};

using namespace luisa::shader;
template<typename Func>
concept ContributionFunc = std::invocable_r<Func, float, Bounding, float4>;

static bool is_light_dirty(
	Image<uint>& light_dirty_map,
	uint light_type,
	uint tlas_index// light or TLAS
) {
	auto bitOffset = tlas_index & 7u;
	tlas_index /= 8u;
	return (light_dirty_map.read(uint2(tlas_index, light_type)).x & (1u << bitOffset)) != 0;
}

static bool is_light_dirty(
	Image<uint>& light_dirty_map,
	uint light_code) {
	auto light_type = light_code >> uint(29);
	auto light_index = light_code & ((uint(1) << uint(29)) - uint(1));
	return is_light_dirty(light_dirty_map, light_type, light_index);
}

HDRISample hdri_importance_sampling(
	float3x3 resource_to_rec2020_mat,
	SpectrumArg& spectrum_arg,
	float3x3 world_2_sky_mat,
	BindlessIndices auto const& bdls_indices,
	float2 rand) {
	if (bdls_indices.sky_heap_idx == max_uint32) {
		HDRISample sample;
		return sample;
	}
	auto sample = sample_hdri(
		world_2_sky_mat,
		bdls_indices.sky_heap_idx,
		bdls_indices.alias_table_idx,
		bdls_indices.pdf_table_idx,
		rand);
	// auto mis_weight = float(no_heuristic ? 1.f : float(sampling::balanced_heuristic(sample.pdf, eval_result.w)));
	sample.L = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * sample.L);
	return sample;
}
bool select_light(
	auto& sampler,
	ContributionFunc auto const& contribution,
	uint mesh_lights_heap_idx,
	uint bvh_heap_idx,
	uint& out_light_type,
	uint& out_light_idx,
	uint& out_tlas_idx,
	float& probability,
	MeshLight& out_mesh_light) {
	probability = 1.0f;
	auto accel_idx = bvh_heap_idx;
	out_light_type = max_uint32;
	out_light_idx = max_uint32;
	out_tlas_idx = max_uint32;
	uint sample_idx;
	sample_idx = 0;
	while (true) {
		auto left_node = g_buffer_heap.buffer_read<BVHNode>(accel_idx, sample_idx);
		auto right_node = g_buffer_heap.buffer_read<BVHNode>(accel_idx, sample_idx + 1);
		auto bd_left = left_node.bounding();
		auto bd_right = right_node.bounding();
		auto cone_left = left_node.cone;
		auto cone_right = right_node.cone;
		float left_rate = contribution(
							  bd_left,
							  cone_left) *
						  left_node.lum;
		float right_rate;
		if (cone_right.w > 1e-8) {
			right_rate = contribution(
							 bd_right,
							 cone_right) *
						 right_node.lum;
		} else {
			right_rate = 0.0f;
		}
		auto sum_rate = right_rate + left_rate;
		if (sum_rate < 1e-8f) {
			return false;
		}
		right_rate /= sum_rate;
		if (right_rate > 1e-8f) {
			right_rate = max(right_rate, 0.02f);
		}
		if (left_rate > 1e-8f) {
			right_rate = min(right_rate, 0.98f);
		}
		left_rate = 1.f - right_rate;
		auto rand = sampler.next();
		auto select_right = rand < (right_rate);
		auto next_idx = select(left_node.index, right_node.index, select_right);
		probability *= select(left_rate, right_rate, select_right);
		if ((next_idx >> uint(29)) != 7u) {
			out_light_idx = next_idx;
			break;
		} else {
			sample_idx = next_idx;
		}
	}
	auto is_accel = (out_light_idx >> uint(29)) == LightTypes::Blas;
	out_light_type = out_light_idx >> uint(29);
	out_light_idx = out_light_idx & ((uint(1) << uint(29)) - uint(1));
	out_tlas_idx = out_light_idx;
	if (is_accel) {
		out_mesh_light = g_buffer_heap.uniform_idx_buffer_read<MeshLight>(
			mesh_lights_heap_idx, out_light_idx);
		accel_idx = out_mesh_light.blas_heap_idx;
		while (true) {
			while (true) {
				auto left_node = g_buffer_heap.buffer_read<BVHNode>(accel_idx, sample_idx);
				auto right_node = g_buffer_heap.buffer_read<BVHNode>(accel_idx, sample_idx + 1);
				auto bd_left = left_node.bounding();
				auto bd_right = right_node.bounding();
				auto cone_left = left_node.cone;
				auto cone_right = right_node.cone;
				float left_rate = contribution(
									  bd_left,
									  cone_left) *
								  left_node.lum;
				float right_rate;
				if (cone_right.w > 1e-8) {
					right_rate = contribution(
									 bd_right,
									 cone_right) *
								 right_node.lum;
				} else {
					right_rate = 0.0f;
				}
				auto sum_rate = right_rate + left_rate;
				if (sum_rate < 1e-8f) {
					out_light_type = max_uint32;
					out_light_idx = max_uint32;
					out_tlas_idx = max_uint32;
					return false;
				}
				right_rate /= sum_rate;
				if (right_rate > 1e-8f) {
					right_rate = max(right_rate, 0.02f);
				}
				if (left_rate > 1e-8f) {
					right_rate = min(right_rate, 0.98f);
				}
				left_rate = 1.f - right_rate;
				auto rand = sampler.next();
				auto select_right = rand < (right_rate);
				auto next_idx = select(left_node.index, right_node.index, select_right);
				probability *= select(left_rate, right_rate, select_right);
				if ((next_idx >> uint(29)) != 7u) {
					out_light_idx = next_idx;
					break;
				} else {
					sample_idx = next_idx;
				}
			}
			is_accel = (out_light_idx >> uint(29)) == LightTypes::Blas;
			out_light_type = out_light_idx >> uint(29);
			out_light_idx = out_light_idx & ((uint(1) << uint(29)) - uint(1));
			if (is_accel) {
				out_mesh_light = g_buffer_heap.uniform_idx_buffer_read<MeshLight>(
					mesh_lights_heap_idx, out_light_idx);
				out_tlas_idx = out_light_idx;
				accel_idx = out_mesh_light.blas_heap_idx;
			} else {
				break;
			}
		}
	}
	return true;
}

static float ray_intersect_sphere(float3 D, float3 O, float3 Center, float R) {
	float a = (D.x * D.x) + (D.y * D.y) + (D.z * D.z);
	float b = (2 * D.x * (O.x - Center.x) + 2 * D.y * (O.y - Center.y) + 2 * D.z * (O.z - Center.z));
	float c = ((O.x - Center.x) * (O.x - Center.x) + (O.y - Center.y) * (O.y - Center.y) + (O.z - Center.z) * (O.z - Center.z)) - R * R;
	float delta = b * b - 4 * a * c;
	if (delta < 0) {
		return -1.0f;
	}
	float t;
	if (delta < 1e-4f) {
		t = -b / (2 * a);
	} else {
		t = min((-b + sqrt(delta)) / (2 * a), (-b - sqrt(delta)) / (2 * a));
	}
	return t;
}
struct LightSample {
	float3 wi;
	float3 L;
	float pdf;
	float mis_weight;
	float wi_length;
};
// xyz: radiance w: ray distance (for denoising)
LightSample light_importance_sampling(
	float3x3 resource_to_rec2020_mat,
	SpectrumArg& spectrum_arg,
	uint& light_type,
	uint& tlas_idx,
	ContributionFunc auto const& contribution,
	auto& sampler,
	float3 world_pos,
	uint light_count) {
	LightSample result;
	if (light_count == 0) return result;
	float rad_contri = 1.f;
	uint light_idx;
	MeshLight mesh_light;
	select_light(
		sampler,
		contribution,
		heap_indices::mesh_lights_heap_idx,
		heap_indices::light_bvh_heap_idx,
		light_type,
		light_idx,
		tlas_idx,
		rad_contri,
		mesh_light);
	switch (light_type) {
		case LightTypes::PointLight: {
			auto point_light = g_buffer_heap.uniform_idx_buffer_read<PointLight>(heap_indices::point_lights_heap_idx, light_idx);
			result.mis_weight = point_light.mis_weight;
			auto wi_light = point_light.pos() - world_pos;
			// triangle: longest edge is c, near angle is b, on angle's other side is a
			auto wi_length = length(wi_light);
			wi_light = wi_light / wi_length;
			auto d_light = wi_length;
			if (wi_length <= point_light.radius()) {
				return result;
			}
			mtl::Onb to_light_onb(wi_light);
			float half_light_angle = asin(point_light.radius() / wi_length);
			float cos_theta_max = cos(half_light_angle);
			float3 cone_dir = sampling::uniform_sample_cone(sampler.next2f(), cos_theta_max);
			wi_light = normalize(to_light_onb.to_world(cone_dir));
			{
				auto t = ray_intersect_sphere(wi_light * wi_length, world_pos, point_light.pos(), point_light.radius());
				if (t < 0) {
					return result;
				}
				wi_length = wi_length * t;
			}
			result.wi = wi_light;
			result.pdf = 1.0f / (2 * pi * (1.0f - cos_theta_max));
			float3 radiance = float3(point_light.radiance);
			radiance = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * radiance);
			result.wi_length = wi_length;
			result.L = radiance / max(1e-10f, rad_contri * (sqr(point_light.radius() * max(0.0f, wi_length - 0.5f)) + 1.0f));
		} break;
		case LightTypes::SpotLight: {
			auto spot_light = g_buffer_heap.uniform_idx_buffer_read<SpotLight>(heap_indices::spot_lights_heap_idx, light_idx);
			result.mis_weight = spot_light.mis_weight;
			auto wi_light = spot_light.pos() - world_pos;
			// triangle: longest edge is c, near angle is b, on angle's other side is a
			auto wi_length = length(wi_light);
			wi_light = wi_light / wi_length;
			auto d_light = wi_length;
			if (wi_length <= spot_light.radius()) {
				return result;
			}
			mtl::Onb to_light_onb(wi_light);
			float half_light_angle = asin(spot_light.radius() / wi_length);
			float cos_theta_max = cos(half_light_angle);
			float3 cone_dir = sampling::uniform_sample_cone(sampler.next2f(), cos_theta_max);
			wi_light = normalize(to_light_onb.to_world(cone_dir));
			float angle = acos(dot(-wi_light, float3(spot_light.forward_dir)));
			if (angle >= spot_light.angle_radian) {
				return result;
			}
			{
				auto t = ray_intersect_sphere(wi_light * wi_length, world_pos, spot_light.pos(), spot_light.radius());
				if (t < 0) {
					return result;
				}
				wi_length = wi_length * t;
			}
			result.wi = wi_light;
			result.pdf = 1.0f / (2 * pi * (1.0f - cos_theta_max));
			float3 radiance = float3(spot_light.radiance);
			radiance = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * radiance);
			result.wi_length = wi_length;
			angle = saturate((angle - spot_light.angle_radian) / (spot_light.small_angle_radian - spot_light.angle_radian));
			angle = pow(angle, spot_light.angle_atten_power);
			result.L = radiance / max(1e-10f, rad_contri * (sqr(spot_light.radius()) * max(0.0f, wi_length - 0.5f) + 1.0f)) * angle;
		} break;
		case LightTypes::AreaLight: {
			auto area_light = g_buffer_heap.uniform_idx_buffer_read<AreaLight>(heap_indices::area_lights_heap_idx, light_idx);
			result.mis_weight = area_light.mis_weight;
			auto light_uv = sampler.next2f();
			auto light_local_pos = light_uv - 0.5f;
			auto p_light = (area_light.transform * float4(light_local_pos, 0.f, 1.f)).xyz;
			auto light_normal = (area_light.transform * float4(0, 0, 1, 0)).xyz;
			light_normal = normalize(light_normal);
			auto pp_light = p_light + light_normal * 1e-5f;
			auto wi_light = pp_light - world_pos;
			auto d_light = length(wi_light);
			wi_light = wi_light / d_light;
			result.wi = wi_light;
			auto cos_light = -dot(light_normal, wi_light);
			result.pdf = (d_light * d_light) / max(area_light.area * cos_light, 1e-5f);
			result.wi_length = d_light;
			auto light_emission = float3(area_light.radiance);
			if (area_light.emission != max_uint32) {
				light_emission *= g_image_heap.image_sample(
												  area_light.emission, light_uv, Filter::LINEAR_POINT, Address::REPEAT)
									  .xyz;
			}
			light_emission = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * light_emission);
			result.L = light_emission / max(1e-5f, rad_contri);
		} break;
		case LightTypes::TriangleLight: {
			if (tlas_idx == max_uint32) {
				return result;
			}
			auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, mesh_light.instance_user_id);
			result.mis_weight = mesh_light.mis_weight;
			auto vertices = geometry::read_vert_pos_uv(g_buffer_heap, light_idx, inst_info.mesh);
			auto light_uvw = sampler.next3f();
			auto light_uvw_len = light_uvw.x + light_uvw.y + light_uvw.z;
			light_uvw /= max(light_uvw_len, 1e-5f);
			for (uint vv = 0; vv < 3; ++vv) {
				vertices[vv].pos = (mesh_light.transform * float4(vertices[vv].pos, 1.)).xyz;
			}
			auto p_light = vertices[0].pos * light_uvw.x + vertices[1].pos * light_uvw.y + vertices[2].pos * light_uvw.z;
			float3 light_normal;
			light_normal = cross(vertices[1].pos - vertices[0].pos, vertices[2].pos - vertices[0].pos);
			light_normal = normalize(light_normal);
			if (dot(light_normal, p_light - world_pos) > 0) {
				light_normal = -light_normal;
			}
			auto pp_light = p_light + light_normal * 1e-5f;
			auto wi_light = pp_light - world_pos;
			auto d_light = length(wi_light);
			wi_light = wi_light / d_light;
			auto cos_light = -dot(light_normal, wi_light);
			auto edge0 = distance(vertices[0].pos, vertices[1].pos);
			auto edge1 = distance(vertices[2].pos, vertices[1].pos);
			auto edge2 = distance(vertices[0].pos, vertices[2].pos);
			auto halen_p = (edge0 + edge1 + edge2) * 0.5f;
			auto light_area = sqrt(halen_p * (halen_p - edge0) * (halen_p - edge1) * (halen_p - edge2));
			result.wi_length = d_light;
			result.pdf = (d_light * d_light) / (light_area * cos_light);
			result.wi = wi_light;
			auto uv0 = vertices[0].uv * light_uvw.x + vertices[1].uv * light_uvw.y + vertices[2].uv * light_uvw.z;
			float3 light_emission = material::get_light_emission(g_buffer_heap, g_image_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, light_idx, uv0);
			// auto col = mis_weight * light_emission / float(max(pdf_light, 1e-5f));
			light_emission = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * light_emission);
			result.L = light_emission / max(1e-5f, rad_contri);
		} break;
		case LightTypes::DiskLight: {
			auto disk_light = g_buffer_heap.uniform_idx_buffer_read<DiskLight>(heap_indices::disk_lights_heap_idx, light_idx);
			result.mis_weight = disk_light.mis_weight;
			mtl::Onb onb(float3(disk_light.forward_dir));
			auto p_light = float3(disk_light.position) + onb.to_world(float3(sampling::sample_uniform_disk(sampler.next2f()), 0.f));
			auto light_normal = float3(disk_light.forward_dir);
			light_normal = normalize(light_normal);
			auto pp_light = p_light + light_normal * 1e-5f;
			auto wi_light = pp_light - world_pos;
			auto d_light = length(wi_light);
			wi_light = wi_light / d_light;
			result.wi = wi_light;
			auto cos_light = -dot(light_normal, wi_light);
			result.pdf = (d_light * d_light) / max(disk_light.area * cos_light, 1e-5f);
			result.wi_length = d_light;
			auto light_emission = float3(disk_light.radiance);
			light_emission = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat * light_emission);
			result.L = light_emission / max(1e-5f, rad_contri);
		} break;
	}
	return result;
}
struct LightISResult {
	float4 radiance;
	float mis;
	uint light_type;
	uint light_index;
};

inline LightISResult bsdf_light_importance_sampling(
	float3x3 resource_to_rec2020_mat,
	SpectrumArg& spectrum_arg,
	auto& bsdf_eval_func,
	auto& sampler,
	float3 world_pos,
	float3 normal,
	float3x3 world_2_sky_mat,
	BindlessIndices auto const& bdls_indices,
	float3 refl_dir,
	float roughness,
	bool is_specular,
	// sample both hdri and local-light
	bool split_di_sample) {
	LightISResult result;
	auto hdri_sample = hdri_importance_sampling(
		resource_to_rec2020_mat,
		spectrum_arg,
		world_2_sky_mat,
		bdls_indices,
		sampler.next2f());

	auto di_sample = light_importance_sampling(
		resource_to_rec2020_mat,
		spectrum_arg,
		result.light_type,
		result.light_index,
		[&](Bounding bound, float4 cone) -> float {
		float3 center = (bound.min + bound.max) * 0.5f;
		float3 box_size = bound.size();
		float box_dist = length(box_size);
		float to_bounding_dist = max(sampling::sd_box(world_pos - center, box_size * 0.5f), 0.02f);
		float3 point_to_light = center + normal * box_dist * 0.5f - world_pos;
		auto point_dist_sqr = dot(point_to_light, point_to_light);
		point_to_light /= sqrt(point_dist_sqr);
		float in_sphere = 1.0f - saturate(dot(point_to_light, normal));
		in_sphere *= in_sphere;
		in_sphere = 1.0f - in_sphere;
		auto angle = dot(-point_to_light, cone.xyz);
		angle = min(cone.w / max(angle, 1e-4f), 1.f);
		float angle_importance;
		angle_importance = 16384.0f * select(angle * angle, 1.f, cone.w > pi * 0.99f);
		float result = angle_importance * in_sphere / to_bounding_dist;
		result *= 1.0f / max(0.02f, point_dist_sqr);
		if (is_specular) {
			result *= pow(saturate(dot(refl_dir, point_to_light)), 1.f / clamp(roughness, 0.4f, 0.9f));
		}
		return result;
	},
		sampler,
		world_pos,
		bdls_indices.light_count);
	float select_weight = 1.f;
	float hdri_lum;
	float di_lum;
	float rand = sampler.next();
	const float epsilon = 1e-5f;
	if (split_di_sample) {
		if (hdri_sample.pdf > epsilon) {
			auto eval_result = bsdf_eval_func(hdri_sample.wi);
			if (eval_result.w > epsilon && !rbc_trace_any(Ray(world_pos, hdri_sample.wi, sampling::offset_ray_t_min, 1e8f), bdls_indices, sampler, ONLY_OPAQUE_MASK)) {
				float hdri_mis = sampling::balanced_heuristic(hdri_sample.pdf, eval_result.w);
				result.radiance.xyz = hdri_sample.L * hdri_mis * eval_result.xyz / max(epsilon, hdri_sample.pdf);
			}
			result.radiance.w = 1024.f;
		}
	} else {
		hdri_lum = reduce_sum(hdri_sample.L) / max(hdri_sample.pdf, epsilon) * sqrt(abs(dot(hdri_sample.wi, normal)));
		di_lum = reduce_sum(di_sample.L) / max(di_sample.pdf, epsilon) * sqrt(abs(dot(di_sample.wi, normal)));
		select_weight = di_lum + hdri_lum;
		if (select_weight <= epsilon) {
			return result;
		}
		select_weight = di_lum / select_weight;
		if (rand < select_weight) {
			result.radiance.w = di_sample.wi_length;
		} else {// sample hdri
			di_sample.mis_weight = 1.f;
			di_sample.L = hdri_sample.L;
			di_sample.pdf = hdri_sample.pdf;
			di_sample.wi = hdri_sample.wi;
			di_sample.wi_length = 1e8f;
			result.radiance.w = 1024.f;
			result.light_type = max_uint32;
			result.light_index = max_uint32;
			select_weight = 1.f - select_weight;
		}
	}
	// shadow
	if (di_sample.pdf < epsilon) {
		result.light_type = max_uint32;
		result.light_index = max_uint32;
		return result;
	}
	auto eval_result = bsdf_eval_func(di_sample.wi);
	if (eval_result.w < epsilon || rbc_trace_any(Ray(world_pos, di_sample.wi, sampling::offset_ray_t_min, max(sampling::offset_ray_t_min, di_sample.wi_length - sampling::offset_ray_t_min)), bdls_indices, sampler, ONLY_OPAQUE_MASK)) {
		result.light_type = max_uint32;
		result.light_index = max_uint32;
		return result;
	}
	result.mis = sampling::weighted_balanced_heuristic(di_sample.mis_weight, di_sample.pdf, eval_result.w);
	float3 di_result = di_sample.L * result.mis * eval_result.xyz / max(epsilon, di_sample.pdf);
	if (split_di_sample) {
		hdri_lum = dot(result.radiance.xyz, float3(1.f)) / max(hdri_sample.pdf, epsilon);
		di_lum = dot(di_result, float3(1.f)) / max(di_sample.pdf, epsilon);
		result.radiance.w = ite(rand < (hdri_lum / max(di_lum + hdri_lum, epsilon)), 1024.f, di_sample.wi_length);
	}
	result.radiance.xyz += di_result / max(epsilon, select_weight);

	return result;
}
}// namespace lighting