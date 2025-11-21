#pragma once
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/read_pixel.hpp>
#include <sampling/sample_funcs.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <utils/onb.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <lighting/importance_sampling.hpp>
//#include "types.hpp"
using namespace luisa::shader;
// struct Reservoir
#include <restir/restir_types.inl>
#define DI_ROUGHNESS_THRESHOLD 0.1f
static float CalcLum(float3 lum) {
	// using acescg color space
	return reduce_sum(lum);
}

static bool get_light_data(
	uint point_lights_heap_idx,
	uint spot_lights_heap_idx,
	uint area_lights_heap_idx,
	uint mesh_lights_heap_idx,
	uint inst_buffer_heap_idx,
	uint mat_idx_buffer_heap_idx,
	uint light_idx,
	float2 uv,
	float3 world_pos,
	uint tlas_index,
	float3& light_color,
	float3& to_light,
	float& dist_to_light,
	float& light_pdf,
	float& mis_weight) {
	using namespace lighting;
	auto light_type = light_idx >> uint(29);
	light_idx = light_idx & ((uint(1) << uint(29)) - uint(1));
	switch (light_type) {
		case LightTypes::PointLight: {
			auto point_light = g_buffer_heap.uniform_idx_buffer_read<PointLight>(point_lights_heap_idx, light_idx);
			mis_weight = point_light.mis_weight;
			auto wi_light = point_light.pos() - world_pos;
			// triangle: longest edge is c, near angle is b, on angle's other side is a
			auto wi_length = length(wi_light);
			wi_light = wi_light / wi_length;
			auto d_light = wi_length;
			if (wi_length <= point_light.radius()) {
				return false;
			}
			mtl::Onb to_light_onb(wi_light);
			float half_light_angle = asin(point_light.radius() / wi_length);
			float cos_theta_max = cos(half_light_angle);
			float3 cone_dir = sampling::uniform_sample_cone(uv, cos_theta_max);
			wi_light = normalize(to_light_onb.to_world(cone_dir));
			{
				auto t = ray_intersect_sphere(wi_light * wi_length, world_pos, point_light.pos(), point_light.radius());
				if (t < 0) return false;
				wi_length = wi_length * t;
			}
			light_color = float3(point_light.radiance);
			to_light = wi_light;
			dist_to_light = wi_length;
			light_pdf = 1.0f / (2 * pi * (1.0f - cos_theta_max));
			return true;
		}
		case LightTypes::SpotLight: {
			auto spot_light = g_buffer_heap.uniform_idx_buffer_read<SpotLight>(spot_lights_heap_idx, light_idx);
			mis_weight = spot_light.mis_weight;
			auto wi_light = spot_light.pos() - world_pos;
			// triangle: longest edge is c, near angle is b, on angle's other side is a
			auto wi_length = length(wi_light);
			wi_light = wi_light / wi_length;
			auto d_light = wi_length;
			if (wi_length <= spot_light.radius()) {
				return false;
			}
			mtl::Onb to_light_onb(wi_light);
			float half_light_angle = asin(spot_light.radius() / wi_length);
			float cos_theta_max = cos(half_light_angle);
			float3 cone_dir = sampling::uniform_sample_cone(uv, cos_theta_max);
			wi_light = normalize(to_light_onb.to_world(cone_dir));
			float angle = acos(dot(-wi_light, float3(spot_light.forward_dir)));
			if (angle >= spot_light.angle_radian) {
				return false;
			}
			{
				auto t = ray_intersect_sphere(wi_light * wi_length, world_pos, spot_light.pos(), spot_light.radius());
				if (t < 0) return false;
				wi_length = wi_length * t;
			}
			angle = saturate((angle - spot_light.angle_radian) / (spot_light.small_angle_radian - spot_light.angle_radian));
			angle = pow(angle, spot_light.angle_atten_power);
			light_color = float3(spot_light.radiance) * angle;
			to_light = wi_light;
			dist_to_light = wi_length;
			light_pdf = 1.0f / (2 * pi * (1.0f - cos_theta_max));
			return true;
		} break;
		case LightTypes::AreaLight: {
			auto area_light = g_buffer_heap.uniform_idx_buffer_read<AreaLight>(area_lights_heap_idx, light_idx);
			mis_weight = area_light.mis_weight;

			light_color = float3(area_light.radiance);
			if (area_light.emission != max_uint32) {
				light_color *= g_image_heap.image_sample(
											   area_light.emission, uv, Filter::LINEAR_POINT, Address::REPEAT)
								   .xyz;
			}

			auto light_local_pos = uv - 0.5f;
			auto light_pos = (area_light.transform * float4(light_local_pos, 0.f, 1.f)).xyz;
			auto light_normal = normalize((area_light.transform * float4(0, 0, 1, 0)).xyz);
			light_pos = sampling::offset_ray_origin(light_pos, light_normal);
			to_light = light_pos - world_pos;
			dist_to_light = length(to_light);
			to_light /= max(dist_to_light, 1e-5f);
			auto cos_light = dot(-to_light, light_normal);
			light_pdf = dist_to_light * dist_to_light / max(area_light.area * cos_light, 1e-5f);
			return cos_light > 1e-4f;
		}
		case LightTypes::TriangleLight: {
			auto mesh_light = g_buffer_heap.uniform_idx_buffer_read<MeshLight>(mesh_lights_heap_idx, tlas_index);
			auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(inst_buffer_heap_idx, mesh_light.instance_user_id);
			mis_weight = mesh_light.mis_weight;
			auto vertices = geometry::read_vert_pos_uv(g_buffer_heap, light_idx, inst_info.mesh);
			float3 light_uvw(uv, 1.0f - uv.x - uv.y);
			for (uint vv = 0; vv < 3; ++vv) {
				vertices[vv].pos = (mesh_light.transform * float4(vertices[vv].pos, 1.)).xyz;
			}
			auto p_light = vertices[0].pos * light_uvw.x + vertices[1].pos * light_uvw.y + vertices[2].pos * light_uvw.z;
			float3 light_normal;
			light_normal = cross(vertices[1].pos - vertices[0].pos, vertices[2].pos - vertices[0].pos);
			light_normal = normalize(light_normal);
			auto pp_light = p_light + light_normal * 1e-5f;
			to_light = pp_light - world_pos;
			dist_to_light = length(to_light);
			to_light = to_light / dist_to_light;
			auto cos_light = -dot(light_normal, to_light);
			auto edge0 = distance(vertices[0].pos, vertices[1].pos);
			auto edge1 = distance(vertices[2].pos, vertices[1].pos);
			auto edge2 = distance(vertices[0].pos, vertices[2].pos);
			auto halen_p = (edge0 + edge1 + edge2) * 0.5f;
			auto light_area = sqrt(halen_p * (halen_p - edge0) * (halen_p - edge1) * (halen_p - edge2));
			light_pdf = (dist_to_light * dist_to_light) / max(1e-5f, light_area * cos_light);
			auto uv0 = vertices[0].uv * light_uvw.x + vertices[1].uv * light_uvw.y + vertices[2].uv * light_uvw.z;
			light_color = material::get_light_emission(g_buffer_heap, g_image_heap, mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, light_idx, uv0);
			return cos_light > 1e-4f;
		}
	}
	return false;
}

struct Reservoir {
	float targetPdf;

	uint M;
	float weightSum;

	uint lightSampledIndex;
	uint lightSampledUV;

	float2 GetLightUV() {
		return float2((lightSampledUV & 0xffff) / float(0xffff), (lightSampledUV >> 16) / float(0xffff));
	}

	void SetLightUV(float2 lightUV) {
		lightSampledUV = (uint(lightUV.x * float(0xffff)) & 0xffff) + (uint(lightUV.y * float(0xffff)) << 16);
	}
	void RefreshUpdate(float p_hat) {
		weightSum = p_hat * targetPdf * M;
	}
	template<typename T>
	inline void initReservoir(uint lightToSample, float weight, T uv) {
		weightSum = weight;
		M = 1;
		lightSampledIndex = lightToSample;
		if constexpr (is_same_v<T, float2>) {
			SetLightUV(uv);
		} else {
			lightSampledUV = uv;
		}
	}
	template<typename T>
	inline bool updateReservoir(uint lightToSample, float weight, float randNumber, T uv) {
		weightSum = weightSum + weight;
		//stream pick
		if (weightSum < 1e-6f || randNumber < weight / weightSum) {
			lightSampledIndex = lightToSample;
			if constexpr (is_same_v<T, float2>) {
				SetLightUV(uv);
			} else {
				lightSampledUV = uv;
			}
			return true;
		}
		return false;
	}
	// void clampReservoir() {
	// 	if (M < 0xffff) {
	// 		weightSum = weightSum / float(M);
	// 		M = 1;
	// 	}
	// }
};

inline void getSkyData(
	BindlessArray& heap,
	float3x3 world_2_sky_mat,
	uint sky_heap_idx,
	uint pdf_table_idx,
	float2 light_uv,
	uint buffer_id,
	float3& toLight, float3& lightIntensity, float& point_pdf) {
	if (sky_heap_idx == max_uint32) {
		toLight = float3(0, 1, 0);
		lightIntensity = float3(0.f);
		point_pdf = 0.f;
		return;
	}
	lightIntensity = heap.image_sample(sky_heap_idx, light_uv).xyz;
	auto p = heap.buffer_read<float>(pdf_table_idx, buffer_id);
	float theta;
	float phi;
	sampling::sphere_uv_to_direction_theta(world_2_sky_mat, light_uv, theta, phi, toLight);
	point_pdf = _directional_pdf(p, theta);
}

inline void getSkyLum(
	BindlessArray& heap,
	float3x3 world_2_sky_mat,
	uint sky_lum_idx,
	uint pdf_table_idx,
	float2 light_uv,
	uint buffer_id,
	float3& toLight, float& lightLum, float& point_pdf) {
	if (sky_lum_idx == max_uint32) {
		toLight = float3(0, 1, 0);
		lightLum = 0.f;
		point_pdf = 0.f;
		return;
	}
	lightLum = heap.image_sample(sky_lum_idx, light_uv).x;
	auto p = heap.buffer_read<float>(pdf_table_idx, buffer_id);
	float theta;
	float phi;
	sampling::sphere_uv_to_direction_theta(world_2_sky_mat, light_uv, theta, phi, toLight);
	point_pdf = _directional_pdf(p, theta);
}

inline void getLightData(
	BindlessArray& heap,
	float3x3 world_2_sky_mat,
	uint sky_heap_idx,
	uint sky_pdf_table_idx,
	uint point_lights_heap_idx,
	uint spot_lights_heap_idx,
	uint area_lights_heap_idx,
	uint mesh_lights_heap_idx,
	uint inst_buffer_heap_idx,
	uint mat_idx_buffer_heap_idx,
	uint light_index,
	uint tlas_index,
	float2 light_uv,
	float3 hitPos, float3& toLight, float3& lightIntensity, float& distToLight, float& light_pdf, float& misWeight) {
	if (light_index == max_uint32) {
		misWeight = 1.0f;
		distToLight = 1e30f;
		if (sky_heap_idx == max_uint32) {
			toLight = float3(0, 1, 0);
			lightIntensity = float3(0);
			light_pdf = 0.f;
			return;
		}
		auto tex_size = heap.image_size(sky_heap_idx);
		auto pixel_id = uint2(light_uv * float2(tex_size));
		getSkyData(heap, world_2_sky_mat, sky_heap_idx, sky_pdf_table_idx, light_uv, pixel_id.x + pixel_id.y * tex_size.x, toLight, lightIntensity, light_pdf);
	} else {
		// local light
		if (!get_light_data(
				heap, point_lights_heap_idx, spot_lights_heap_idx, area_lights_heap_idx, mesh_lights_heap_idx, inst_buffer_heap_idx, mat_idx_buffer_heap_idx, light_index, light_uv,
				hitPos, tlas_index, lightIntensity, toLight, distToLight, light_pdf, misWeight)) {
			light_pdf = 0;
			misWeight = 0;
		}
	}
}

trait_struct PDFType {
	static constexpr uint32 NdotL = 0;
	static constexpr uint32 GGX = 1;
	static constexpr uint32 TransmissionNdotL = 2;
	static constexpr uint32 TransmissionGGX = 3;
};

static float computePdf(
	float3 viewDir,// normalize(dest_position - self_position)
	float3 normal,
	float3 lightDir,// normalize(light_positon - self_position)
	float roughness,
	uint32 pdfType) {
	switch (pdfType) {
		case PDFType::NdotL: {
			return max(dot(lightDir, normal), 0.f);
		}
		case PDFType::TransmissionNdotL: {
			return max(-dot(lightDir, normal), 0.f);
		}
		case PDFType::GGX: {
			mtl::Onb onb(normal);
			float3 local_view_dir = onb.to_local(viewDir);
			float3 local_wi = onb.to_local(lightDir);
			float3 h = normalize(local_wi + local_view_dir);
			auto pdf = max(0.f, sampling::ggx_pdf_no_pi(roughness, local_view_dir, h));
			pdf *= ite(dot(lightDir, normal) <= 1e-5f, 0.f, 1.f);
			return pdf;
		}
		case PDFType::TransmissionGGX: {
			mtl::Onb onb(-viewDir);
			float3 local_view_dir(0, 0, 1);
			float3 local_wi = onb.to_local(lightDir);
			float3 h = normalize(local_wi + local_view_dir);
			auto pdf = max(0.f, sampling::ggx_pdf_no_pi(roughness, local_view_dir, h));
			pdf *= ite(dot(lightDir, -normal) <= 1e-5f, 0.f, 1.f);
			return pdf;
		}
		default:
			return 0.0f;
	}
}

// #define DEBUG_SAMPLE_HDRI