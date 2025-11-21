#pragma once
/////////// Allowed macros:
// #define PT_CONFIDENCE_ACCUM
// #define PT_SCREEN_RAY_DIFF
// #define PT_MOTION_VECTORS
#include <bsdfs/polymorphic.hpp>
#include <geometry/vertices.hpp>
#include <luisa/std.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <utils/onb.hpp>
#include <lighting/importance_sampling.hpp>
#include <spectrum/spectrum.hpp>
#include <material/mats.hpp>
using namespace luisa::shader;

constexpr float const denoise_min_albedo = 1e-1f;
inline float3 clamp_denoise_albedo(float3 albedo) {
	float max_val = max(albedo.x, max(albedo.y, albedo.z)) * denoise_min_albedo;
	return max(albedo, float3(max_val));
}
struct IntegratorResult {
	float3 world_pos;
	float3 new_ray_offset;
#ifdef PT_MOTION_VECTORS
	float3 last_local_pos;
#endif
	float3 plane_normal;// cross(v0 - v1, v0 - v2)
	float3 normal;		// bump texture affected normal
	float3 emission = 0.0f;
	float3 albedo = denoise_min_albedo;

#ifndef OFFLINE_MODE
	float3 specular_albedo = denoise_min_albedo;
#endif

	float roughness;
	uint user_id;
	mtl::BSDFFlags sample_flags = mtl::BSDFFlags::None;
};
namespace integrator {

}// namespace integrator

static IntegratorResult sample_material(
	auto& sampler,
	auto& volume_stack,
	float& length_sum,
	auto& hit,
	auto procedural_geometry,
	SpectrumArg& spectrum_arg,
	float3x3 world_2_sky_mat,
	vt::VTMeta vt_meta,
	float lobe_rand,
	float3 input_pos,
	float3& beta,
	bool& continue_loop,
	bool need_albedo,
	//////////// texture grad
	uint& texture_filter,
#ifdef PT_SCREEN_RAY_DIFF
	float2 tex_grad_scale,
#endif
	float2& ddx,
	float2& ddy,
#ifdef PT_SCREEN_RAY_DIFF
	float3 up_ray_dir,
	float3 right_ray_dir,
#endif

//////////// mis
#ifdef PT_CONFIDENCE_ACCUM
	Image<uint>& light_dirty_map,
#endif
	mtl::ShadingDetail& detail,
	lighting::BindlessIndices auto const& bdls_indices,
//////////// out
#ifdef PT_CONFIDENCE_ACCUM
	float& confidence,
	float sky_confidence,
#endif
	bool importance_sampling,
	float3x3 resource_to_rec2020_mat,
	float3& di_result,
	float& di_dist,
	float& pdf_bsdf,
	float3& new_dir
#ifdef PT_MOTION_VECTORS
	,
	bool require_last_local_pos
#endif
	,
	bool& reject) {
	material::MatMeta mat_meta;
	geometry::InstanceInfo inst_info;
	bool is_indirect_ray = detail != mtl::ShadingDetail::Default;
	auto user_id = g_accel.instance_user_id(hit.inst);
	float3 world_pos;
	float2 uv;
	float3 vertices_normal;
	float3 plane_normal;
	float3 input_dir;
	float ray_t;
	mtl::Onb vertices_onb;
	float4x4 inst_transform;
	bool contained_normal = false;
	bool contained_tangent = false;
	mtl::openpbr::BasicParameter basic_param;
	std::array<float3, 3> vert_poses;
	std::array<float3, 3> vert_normals;
#ifdef PT_MOTION_VECTORS
	float3 last_local_pos;
#endif
	bool hit_triangle = true;
	if constexpr (requires { hit.hit_triangle(); }) {
		hit_triangle = hit.hit_triangle();
	}
	if (hit_triangle) {
		inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
		uint contained_uv = 0;
		geometry::Triangle triangle;
		auto vertices = geometry::read_vertices(g_buffer_heap, hit.prim, inst_info.mesh, contained_normal, contained_tangent, contained_uv, triangle);
		mat_meta = material::mat_meta(g_buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, hit.prim);
		inst_transform = g_accel.instance_transform(hit.inst);
		auto local_pos = hit.interpolate(vertices[0].pos, vertices[1].pos, vertices[2].pos);
#ifdef PT_MOTION_VECTORS
		last_local_pos = local_pos;
		if (require_last_local_pos && inst_info.last_vertex_heap_idx != uint(-1)) {
			float3 p0 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[0]);
			float3 p1 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[1]);
			float3 p2 = g_buffer_heap.buffer_read<float3>(inst_info.last_vertex_heap_idx, triangle[2]);
			last_local_pos = hit.interpolate(p0, p1, p2);
		}
#endif
		for (int i = 0; i < 3; ++i) {
			vert_poses[i] = (inst_transform * float4(vertices[i].pos, 1)).xyz;
			vert_normals[i] = vertices[i].normal;
		}
		plane_normal = cross(vert_poses[0] - vert_poses[1], vert_poses[0] - vert_poses[2]);
		plane_normal = normalize(plane_normal);
		world_pos = (inst_transform * float4(local_pos, 1)).xyz;
		uv = hit.interpolate(vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);

		input_dir = world_pos - input_pos;
		ray_t = length(input_dir);
		hit.ray_t = ray_t;
		input_dir = input_dir / ray_t;

#ifdef PT_SCREEN_RAY_DIFF
		if (!is_indirect_ray) {
			auto right_bary = sampling::ray_tri_bary(
				input_pos,
				right_ray_dir,
				vert_poses[0],
				vert_poses[1],
				vert_poses[2]);
			auto up_bary = sampling::ray_tri_bary(
				input_pos,
				up_ray_dir,
				vert_poses[0],
				vert_poses[1],
				vert_poses[2]);
			auto right_uv = interpolate(right_bary, vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);
			auto up_uv = interpolate(up_bary, vertices[0].uvs[0], vertices[1].uvs[0], vertices[2].uvs[0]);
			ddx = (right_uv - uv) * tex_grad_scale.x;
			ddy = (up_uv - uv) * tex_grad_scale.y;
		}
#endif

		if (contained_normal) {
			vertices_normal = hit.interpolate(vertices[0].normal, vertices[1].normal, vertices[2].normal);
			vertices_normal = normalize((inst_transform * float4(vertices_normal, 0)).xyz);
			if (dot(input_dir, plane_normal) * dot(input_dir, vertices_normal) < 0.0f) {
				vertices_normal = plane_normal;
			}
		} else {
			vertices_normal = plane_normal;
		}

		if (contained_tangent) {
			auto tangent = hit.interpolate(vertices[0].tangent, vertices[1].tangent, vertices[2].tangent);
			basic_param.geometry.onb.tangent = normalize(inst_transform * float4(tangent.xyz, 0)).xyz;
			basic_param.geometry.onb.bitangent = normalize(cross(vertices_normal, tangent.xyz));
			basic_param.geometry.onb.normal = vertices_normal;
		} else {
			basic_param.geometry.onb = mtl::Onb(vertices_normal);
		}

		vertices_onb = basic_param.geometry.onb;

		continue_loop = transform_to_params(
			g_buffer_heap,
			g_image_heap,
			mat_meta,
			basic_param,
			texture_filter,
			vt_meta,
			uv,
			float4(ddx, ddy),
			input_dir,
			world_pos,
			reject,
			bdls_indices.time);
		if (basic_param.geometry.thin_walled && dot(input_dir, vertices_normal) >= 0.0f) {
			basic_param.geometry.onb.normal = -basic_param.geometry.onb.normal;
			vertices_onb.normal = -vertices_onb.normal;
		}
	} else {
		plane_normal = procedural_geometry.normal;
		vertices_normal = plane_normal;
		ray_t = hit.ray_t;
		world_pos = (ray_t - 1e-3f) * new_dir + input_pos + vertices_normal * 1e-4f;
		input_dir = normalize(world_pos - input_pos);
		basic_param.geometry.onb = mtl::Onb(vertices_normal);
		vertices_onb = basic_param.geometry.onb;
		continue_loop = ray_t > 0;
	}

	auto init_spectrum_colors = [&](auto& param) {
		if constexpr (requires { param.emission; })
			param.emission.luminance.init_emission(g_image_heap, g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.base; })
			param.base.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.specular; })
			param.specular.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.diffraction; })
			if (basic_param.weight.diffraction > 0.0f)
				param.diffraction.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.coat; })
			if (basic_param.weight.coat > 0.0f)
				param.coat.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.fuzz; })
			if (basic_param.weight.fuzz > 0.0f)
				param.fuzz.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
		if constexpr (requires { param.subsurface; })
			if (basic_param.weight.subsurface > 0.0f) {
				param.subsurface.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
				param.subsurface.radius = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, param.subsurface.radius);
			}
		if constexpr (requires { param.transmission; })
			if (basic_param.weight.transmission > 0.0f) {
				param.transmission.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
				if (any(param.transmission.scatter != 0.0f)) {
					param.transmission.scatter = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, param.transmission.scatter);
				}
			}
	};

	init_spectrum_colors(basic_param);

	float3 wi = -basic_param.geometry.onb.to_local(input_dir);

	return mtl::PolymorphicBSDF::visit(std::to_underlying(mtl::detect_polymorphic_bsdf_type(basic_param.weight)), [&]<class ins>() {
		using type_pairs = typename ins::type;
		using MatBSDF = typename type_pairs::first_type;
		using MatExtraParameter = typename type_pairs::second_type;

		IntegratorResult r;

#ifdef PT_MOTION_VECTORS
		r.last_local_pos = last_local_pos;
#endif
		r.world_pos = world_pos;
		r.new_ray_offset = 0.0f;
		r.user_id = user_id;
		r.normal = basic_param.geometry.onb.normal;
		r.plane_normal = plane_normal;
		r.roughness = basic_param.specular.roughness * (1.0f - 0.8f * basic_param.specular.roughness_anisotropy);

		MatExtraParameter extra_param;

		if constexpr (requires { extra_param.coat; }) {
			extra_param.coat.coat_onb = vertices_onb;
		}
		if (hit_triangle) {
			transform_to_params(
				g_buffer_heap,
				g_image_heap,
				mat_meta,
				extra_param,
				texture_filter,
				vt_meta,
				uv,
				float4(ddx, ddy),
				input_dir,
				world_pos,
				reject,
				bdls_indices.time);
		} else {
			if constexpr (requires { extra_param.base; }) {
				extra_param.base.color = float3(0.3f, 0.6f, 0.7f);
			}
		}

		init_spectrum_colors(extra_param);

		if (basic_param.geometry.thin_walled || dot(input_dir, basic_param.geometry.onb.normal) < 0) {
			r.emission = basic_param.emission.luminance.spectral();
			if constexpr (requires { extra_param.coat; }) {
				r.emission *= lerp(float3(1.0f), extra_param.coat.color.spectral(), basic_param.weight.coat);
			}
		}

		mtl::ClosureData<MatExtraParameter> closure_data{basic_param, extra_param, spectrum_arg.lambda, detail};

		if (wi.z > 0.0f) {
			if (!volume_stack.empty()) {
				closure_data.inv_out_ior = rcp(volume_stack.back().ior);
			}
		} else {
			if (volume_stack.size() > 1) {
				closure_data.inv_out_ior = rcp(volume_stack[volume_stack.size() - 2].ior);
			}
		}

		closure_data.selected_wavelength = spectrum_arg.selected_wavelength;
		closure_data.hero_wavelength_index = spectrum_arg.hero_index;
		closure_data.ray_t = ray_t;
		if (length_sum > 0.0f) {
			closure_data.ray_t += length_sum;
		}

		closure_data.rand = float3(sampler.next2f(g_buffer_heap), lobe_rand);
		closure_data.init(basic_param, extra_param);

		MatBSDF bsdf;

		bool di_use_specular = false;

		auto bsdf_eval_func = [&](float3 light_dir) {
			float3 wo = basic_param.geometry.onb.to_local(light_dir);

			auto old_flags = closure_data.sampling_flags;
			if (dot(vertices_normal, light_dir) *
					dot(vertices_normal, input_dir) <
				0) {
				if (!mtl::is_reflective(r.sample_flags)) return float4{0.0f};
				closure_data.sampling_flags = mtl::BSDFFlags(old_flags & mtl::BSDFFlags::Reflection);
			} else {
				if (!mtl::is_transmissive(r.sample_flags)) return float4{0.0f};
				closure_data.sampling_flags = mtl::BSDFFlags(old_flags & mtl::BSDFFlags::Transmission);
			}
			auto eval_result = bsdf.eval(wi, wo, closure_data);
			auto pdf = bsdf.pdf(wi, wo, closure_data);
			closure_data.sampling_flags = old_flags;

			if (!eval_result ||
				any(eval_result.val < 0.f) ||
				!any(is_finite(eval_result.val)) ||
				!is_finite(pdf) || pdf <= 0.0f) {
				return float4{0.0f};
			}
			return float4{eval_result.val, pdf};
		};
		if (continue_loop) {
			bsdf.init(wi, basic_param, extra_param, closure_data);
			if (need_albedo) {
				bool oldFlag = closure_data.spectrumed;
				closure_data.spectrumed = false;

#ifndef OFFLINE_MODE
				auto old_flags = closure_data.sampling_flags;
				closure_data.sampling_flags = mtl::BSDFFlags(old_flags & mtl::BSDFFlags::Diffuse);
				r.albedo = clamp_denoise_albedo(bsdf.energy(wi, closure_data));
				closure_data.sampling_flags = mtl::BSDFFlags(old_flags & (mtl::BSDFFlags::Specular | mtl::BSDFFlags::Delta));
				r.specular_albedo = clamp_denoise_albedo(bsdf.energy(wi, closure_data));
				closure_data.sampling_flags = old_flags;
#else
				r.albedo = bsdf.energy(wi, closure_data);
#endif
				closure_data.spectrumed = oldFlag;
			}

			closure_data.rand = float3(sampler.next2f(g_buffer_heap), lobe_rand);
			auto sample_result = bsdf.sample(wi, closure_data, volume_stack);
			spectrum_arg.selected_wavelength = closure_data.selected_wavelength;
			r.sample_flags = sample_result.throughput.flags;
			if (!sample_result ||
				any(sample_result.throughput.val < 0.f) ||
				!any(is_finite(sample_result.throughput.val)) ||
				!is_finite(sample_result.pdf)) {
				continue_loop = false;
				beta = 0.0f;
				return r;
			}
			new_dir = basic_param.geometry.onb.to_world(sample_result.wo);
			if (
				(dot(vertices_normal, input_dir) *
				 dot(vertices_normal, new_dir) *
				 (is_reflective(sample_result.throughput.flags) * 2 - 1)) > 0) {
				new_dir -= 2.0f * dot(vertices_normal, new_dir) * vertices_normal;
			}

			pdf_bsdf = max(1e-4f, sample_result.pdf);
			beta *= sample_result.throughput.val / pdf_bsdf;

			if (mtl::is_delta(sample_result.throughput.flags)) pdf_bsdf = -1.0f;

			if (mtl::is_specular(sample_result.throughput.flags)) {
				di_use_specular = true;
			}
			if (mtl::is_transmissive(sample_result.throughput.flags)) {
				length_sum = -1.0f;
				if (basic_param.geometry.thin_walled)
					r.new_ray_offset = -basic_param.geometry.onb.normal * basic_param.geometry.thickness;
			}
			if (mtl::is_non_delta(sample_result.throughput.flags) && sample_result.throughput.flags != mtl::BSDFFlags::SpecularTransmission) {
				detail = max(detail, mtl::is_specular(sample_result.throughput.flags) ? mtl::ShadingDetail::IndirectSpecular : mtl::ShadingDetail::IndirectDiffuse);
			}
		}

		if (is_indirect_ray && hit_triangle) {// indirect ray
			//////////////// Sample light and balance
			uint light_mask = inst_info.get_light_mask();
			uint light_id = inst_info.get_light_id();
			switch (light_mask) {
				case lighting::LightTypes::PointLight: {
					auto point_light = g_buffer_heap.uniform_idx_buffer_read<lighting::PointLight>(heap_indices::point_lights_heap_idx, light_id);
					// triangle: longest edge is c, near angle is b, on angle's other side is a
					auto wi_length = length(point_light.pos() - input_pos);
					if (wi_length > point_light.radius()) {
						float half_light_angle = asin(point_light.radius() / wi_length);
						float cos_theta_max = cos(half_light_angle);
						auto pdf_light = 1.0f / (2 * pi * (1.0f - cos_theta_max));
						auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
						r.emission *= mis;
#ifdef PT_CONFIDENCE_ACCUM
						if (lighting::is_light_dirty(light_dirty_map, light_mask, light_id)) {
							confidence *= 1.0f - mis;
						}
#endif
					}
					continue_loop = false;
				} break;
				case lighting::LightTypes::SpotLight: {
					auto spot_light = g_buffer_heap.uniform_idx_buffer_read<lighting::SpotLight>(heap_indices::spot_lights_heap_idx, light_id);
					auto wi_light = spot_light.pos() - input_pos;
					// triangle: longest edge is c, near angle is b, on angle's other side is a
					auto wi_length = length(wi_light);
					wi_light = wi_light / wi_length;
					if (wi_length > spot_light.radius()) {
						float half_light_angle = asin(spot_light.radius() / wi_length);
						float cos_theta_max = cos(half_light_angle);
						auto pdf_light = 1.0f / (2 * pi * (1.0f - cos_theta_max));
						auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
						float angle = acos(dot(-input_dir, float3(spot_light.forward_dir)));
						angle = saturate((angle - spot_light.angle_radian) / (spot_light.small_angle_radian - spot_light.angle_radian));
						angle = pow(angle, spot_light.angle_atten_power);
						r.emission *= angle;
						r.emission *= mis;
#ifdef PT_CONFIDENCE_ACCUM
						if (lighting::is_light_dirty(light_dirty_map, light_mask, light_id)) {
							confidence *= 1.0f - mis;
						}
#endif
					}
					continue_loop = false;
				} break;
				case lighting::LightTypes::AreaLight: {
					float a = distance(vert_poses[0], vert_poses[1]);
					float b = distance(vert_poses[0], vert_poses[2]);
					float c = distance(vert_poses[1], vert_poses[2]);
					float p = (a + b + c) / 2.0f;
					float area = sqrt(p * (p - a) * (p - b) * (p - c)) * 2.f;
					auto pdf_light = (ray_t * ray_t) / max(area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
					auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
					r.emission *= mis;
#ifdef PT_CONFIDENCE_ACCUM
					if (lighting::is_light_dirty(light_dirty_map, light_mask, light_id)) {
						confidence *= 1.0f - mis;
					}
#endif
				} break;
				case lighting::LightTypes::TriangleLight: {
					float a = distance(vert_poses[0], vert_poses[1]);
					float b = distance(vert_poses[0], vert_poses[2]);
					float c = distance(vert_poses[1], vert_poses[2]);
					float p = (a + b + c) / 2.0f;
					float area = sqrt(p * (p - a) * (p - b) * (p - c));
					auto pdf_light = (ray_t * ray_t) / max(area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
					auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
					r.emission *= mis;
#ifdef PT_CONFIDENCE_ACCUM
					if (lighting::is_light_dirty(light_dirty_map, light_mask, light_id)) {
						confidence *= 1.0f - mis;
					}
#endif
				} break;
				case lighting::LightTypes::DiskLight: {
					auto disk_light = g_buffer_heap.uniform_idx_buffer_read<lighting::DiskLight>(heap_indices::disk_lights_heap_idx, light_id);
					auto pdf_light = (ray_t * ray_t) / max(disk_light.area * max(dot(-input_dir, vertices_normal), 0.f), 1e-5f);
					auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
					r.emission *= mis;
#ifdef PT_CONFIDENCE_ACCUM
					if (lighting::is_light_dirty(light_dirty_map, light_mask, light_id)) {
						confidence *= 1.0f - mis;
					}
#endif
				} break;
			}
			// radiance += beta * emission;
			//////////////// Done sample light and balance
		}

		if (!continue_loop) {
			return r;
		}

		lighting::LightISResult is_result;
		///////////// IS
		if (importance_sampling && pdf_bsdf > 1e-4f) {
			bool need_flip = false;
			float3 offset_normal = plane_normal;
			float3 di_normal = r.normal;
			if (dot(new_dir, vertices_normal) < 0.f) {
				di_normal = -di_normal;
				offset_normal = -offset_normal;
				need_flip = true;
			}
			float3 di_world_pos = world_pos;
			if (contained_normal) {
				// RAY TRACING GEMS II
				// CHAPTER 4. HACKING THE SHADOW TERMINATOR
				float3 nA = normalize((inst_transform * float4(vert_normals[0], 0)).xyz);
				float3 nB = normalize((inst_transform * float4(vert_normals[1], 0)).xyz);
				float3 nC = normalize((inst_transform * float4(vert_normals[2], 0)).xyz);
				if (need_flip) {
					nA = -nA;
					nB = -nB;
					nC = -nC;
				}
				// get distance vectors from triangle vertices
				float3 tmpu = di_world_pos - vert_poses[0];
				float3 tmpv = di_world_pos - vert_poses[1];
				float3 tmpw = di_world_pos - vert_poses[2];
				// project these onto the tangent planes
				// defined by the shading normals
				tmpu -= min(0.0f, dot(tmpu, nA)) * nA;
				tmpv -= min(0.0f, dot(tmpv, nB)) * nB;
				tmpw -= min(0.0f, dot(tmpw, nC)) * nC;
				// finally P' is the barycentric mean of these three
				di_world_pos += hit.interpolate(tmpu, tmpv, tmpw);
			}
			di_world_pos = sampling::offset_ray_origin(di_world_pos, offset_normal);

			is_result = lighting::bsdf_light_importance_sampling(
				resource_to_rec2020_mat,
				spectrum_arg,
				bsdf_eval_func,
				sampler,
				di_world_pos,
				di_normal,
				world_2_sky_mat,
				bdls_indices,
				new_dir,
				r.roughness,
				di_use_specular,
				!is_indirect_ray);
			float hdri_mis = 1;
			di_result = is_result.radiance.xyz;
			float rand_num = sampler.next(g_buffer_heap);
			di_dist = is_result.radiance.w;
			// confidence
#ifdef PT_CONFIDENCE_ACCUM
			if (is_result.light_type < 7 && reduce_sum(is_result.radiance.xyz) > 1e-5f) {
				if (lighting::is_light_dirty(light_dirty_map, is_result.light_type, is_result.light_index)) {
					confidence *= 1.0f - is_result.mis;
				}
			}
			// if (is_result.hdri_sample_pdf > 1e-8f) {
			confidence *= sky_confidence;
			// }
#endif
			return r;
		} else {
			di_result = float3(0);
		}
		return r;
	});
}
