#pragma once

#include <geometry/vertices.hpp>
#include <bsdfs/base/utils.hpp>
#include <material/mats.hpp>
#include <sampling/sample_funcs.hpp>
#include <std/inplace_vector>
#include <std/type_traits>
#include <path_tracer/trace.hpp>
#include <volumetric/volume.hpp>

namespace mtl {

using namespace luisa::shader;

constexpr float3 MEDIUM_PROBE_DIRECTION = float3(0.0f, 1.0f, 0.0f);
constexpr uint MAX_MEDIUM_BOUNDARY_STEPS = 64u;

struct MediumProbeBoundary {
	Volume volume;
	float dispersion_scale = 0.0f;
	float dispersion_abbe_number = 20.0f;
	bool is_medium = false;
	bool entering = false;
};

static MediumProbeBoundary probe_triangle_medium(
	CommittedHit hit,
	float3 ray_dir,
	SpectrumArg& spectrum_arg,
	float3x3 resource_to_rec2020_mat) {
	MediumProbeBoundary result;
	auto user_id = g_accel.instance_user_id(hit.inst);
	auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(
		heap_indices::inst_buffer_heap_idx,
		user_id);
	auto mat_meta = material::mat_meta(
		g_buffer_heap,
		heap_indices::mat_idx_buffer_heap_idx,
		inst_info.mesh.submesh_heap_idx,
		inst_info.mat_index,
		hit.prim);

	result = material::PolymorphicMaterial::visit(mat_meta.mat_type, [&]<class ins>() {
		MediumProbeBoundary boundary;
		using Type = typename ins::type;
		if constexpr (std::is_same_v<Type, material::OpenPBR>) {
			auto weight = g_buffer_heap.uniform_idx_byte_buffer_read<material::OpenPBR::Weight>(
				ins::index,
				mat_meta.mat_index * sizeof(material::OpenPBR) + offsetof(material::OpenPBR, weight));
			auto geometry = g_buffer_heap.uniform_idx_byte_buffer_read<material::OpenPBR::Geometry>(
				ins::index,
				mat_meta.mat_index * sizeof(material::OpenPBR) + offsetof(material::OpenPBR, geometry));
			if ((weight.transmission > 0.0f || weight.subsurface > 0.0f) &&
				!geometry.thin_walled) {
				auto specular = g_buffer_heap.uniform_idx_byte_buffer_read<material::OpenPBR::Specular>(
					ins::index,
					mat_meta.mat_index * sizeof(material::OpenPBR) + offsetof(material::OpenPBR, specular));

				if (weight.transmission > 0.0f) {
					auto transmission = g_buffer_heap.uniform_idx_byte_buffer_read<material::OpenPBR::Transmission>(
						ins::index,
						mat_meta.mat_index * sizeof(material::OpenPBR) + offsetof(material::OpenPBR, transmission));
					openpbr::Parameter::Transmission params;
					params.color = spectrum::SpectrumColor(float3(transmission.transmission_color));
					params.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
					params.depth = transmission.transmission_depth;
					params.scatter = float3(transmission.transmission_scatter);
					if (any(params.scatter != 0.0f)) {
						params.scatter = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, params.scatter);
					}
					params.scatter_anisotropy = transmission.transmission_scatter_anisotropy;
					boundary.volume.fill_from_transmission(params);
					boundary.volume.ior = specular.ior;
					boundary.dispersion_scale = transmission.transmission_dispersion_scale;
					boundary.dispersion_abbe_number = transmission.transmission_dispersion_abbe_number;
				} else {
					auto subsurface = g_buffer_heap.uniform_idx_byte_buffer_read<material::OpenPBR::Subsurface>(
						ins::index,
						mat_meta.mat_index * sizeof(material::OpenPBR) + offsetof(material::OpenPBR, subsurface));
					openpbr::Parameter::Subsurface params;
					params.color = spectrum::SpectrumColor(float3(subsurface.subsurface_color));
					params.color.init_reflectance(g_volume_heap, spectrum_arg, resource_to_rec2020_mat);
					params.radius = float3(subsurface.subsurface_radius_scale) * subsurface.subsurface_radius;
					params.radius = spectrum::reflectance_to_spectrum(g_volume_heap, spectrum_arg, params.radius);
					params.scatter_anisotropy = subsurface.subsurface_scatter_anisotropy;
					boundary.volume.fill_from_subsurface(params);
					boundary.volume.ior = specular.ior;
				}
				boundary.volume.boundary_id = hit.inst;
				boundary.volume.nested_priority = geometry.nested_priority;
				boundary.is_medium = true;
			}
		}
		return boundary;
	});

	if (!result.is_medium) return result;

	bool contained_normal;
	bool contained_tangent;
	uint uv_count;
	geometry::Triangle triangle;
	auto vertices = geometry::read_vertices(
		g_buffer_heap,
		hit.prim,
		inst_info.mesh,
		contained_normal,
		contained_tangent,
		uv_count,
		triangle);
	auto inst_transform = g_accel.instance_transform(hit.inst);
	std::array<float3, 3> world_pos;
	for (uint i = 0; i < 3; ++i) {
		world_pos[i] = (inst_transform * float4(vertices[i].pos, 1.0f)).xyz;
	}
	auto plane_normal = normalize(cross(
		world_pos[0] - world_pos[1],
		world_pos[0] - world_pos[2]));
	result.entering = dot(ray_dir, plane_normal) < 0.0f;
	return result;
}

static bool probe_medium_stack(
	ActiveMediumList& volume_stack,
	float3 origin,
	TraceIndices auto const& idxs,
	auto& rng,
	SpectrumArg& spectrum_arg,
	float3x3 resource_to_rec2020_mat,
	uint mask = max_uint32) {
	std::array<uint, Volume::MAX_VOLUME_STACK_SIZE> forward_boundaries;
	uint forward_count = 0u;
	bool reached_miss = false;
	bool overflow = false;
	bool select_wavelength = spectrum_arg.selected_wavelength;
	constexpr float3 direction = MEDIUM_PROBE_DIRECTION;
	Ray probe_ray(origin, direction, sampling::offset_ray_t_min);
	ProceduralGeometry procedural_geometry;

	volume_stack.clear();
	for (uint step = 0u; step < MAX_MEDIUM_BOUNDARY_STEPS; ++step) {
		auto hit = rbc_trace_closest(probe_ray, idxs, rng, procedural_geometry, mask);
		if (hit.miss()) {
			reached_miss = true;
			break;
		}
		if (hit.hit_triangle()) {
			auto boundary = probe_triangle_medium(
				hit,
				direction,
				spectrum_arg,
				resource_to_rec2020_mat);
			if (boundary.is_medium) {
				auto boundary_id = boundary.volume.boundary_id;
				if (boundary.entering) {
					if (forward_count < Volume::MAX_VOLUME_STACK_SIZE) {
						forward_boundaries[forward_count++] = boundary_id;
					} else {
						overflow = true;
					}
				} else {
					int forward_index = -1;
					if (forward_count > 0u && forward_boundaries[forward_count - 1u] == boundary_id) {
						forward_index = int(forward_count - 1u);
					} else if (forward_count == 2u && forward_boundaries[0] == boundary_id) {
						forward_index = 0;
					}
					if (forward_index >= 0) {
						if (forward_index == 0 && forward_count == 2u) {
							forward_boundaries[0] = forward_boundaries[1];
						}
						--forward_count;
					} else {
						if (boundary.dispersion_scale > 0.0f) {
							boundary.volume.ior = dispersion_ior(
								boundary.volume.ior,
								boundary.dispersion_abbe_number,
								boundary.dispersion_scale,
								spectrum_arg.lambda[spectrum_arg.hero_index]);
							select_wavelength = true;
						}
						if (!active_medium_insert_enclosing(
								volume_stack,
								boundary.volume)) {
							overflow = true;
						}
					}
				}
			}
		}
		if (overflow) break;

		auto hit_pos = probe_ray.origin() + max(hit.ray_t, sampling::offset_ray_t_min) * direction;
		probe_ray.set_origin(sampling::offset_ray_origin(hit_pos, direction));
		probe_ray.t_min = sampling::offset_ray_t_min;
		probe_ray.t_max = 1e30f;
	}
	bool complete = reached_miss && !overflow && forward_count == 0u;
	if (!complete) {
		volume_stack.clear();
		return false;
	}
	if (select_wavelength) {
		spectrum_arg.selected_wavelength = true;
		spectrum_arg.lambda = spectrum_arg.lambda[spectrum_arg.hero_index];
		collapse_active_medium_wavelengths(volume_stack, spectrum_arg.hero_index);
	}
	return true;
}

}// namespace mtl
