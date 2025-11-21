#pragma once
#include <luisa/std.hpp>
#include <luisa/resources/common_extern.hpp>
#include <geometry/types.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <virtual_tex/stream.hpp>
#include <std/concepts>

namespace luisa::shader {
#define RBQ_USE_RAYQUERY
#define RBQ_USE_RAYQUERY_SHADOW

#if defined(RBQ_USE_RAYQUERY) || defined(RBQ_USE_RAYQUERY_SHADOW)
extern Buffer<uint>& g_triangle_vis_buffer;
#endif
extern Accel& g_accel;
#define DEFINED_G_ACCEL
}// namespace luisa::shader

#include <sampling/procedural_sampling.hpp>

using namespace luisa::shader;

template<class T>
concept TraceIndices = requires(T t) {
	{ auto(t.frame_countdown) } -> std::same_as<uint>;
};

#if defined(RBQ_USE_RAYQUERY) || defined(RBQ_USE_RAYQUERY_SHADOW)
static bool commit_triangle(TriangleHit hit, TraceIndices auto const& idxs, auto& rng) {
	auto user_id = g_accel.instance_user_id(hit.inst);
	auto heap_idx = g_triangle_vis_buffer.read(user_id);
	if (heap_idx != max_uint32) {
		auto value = (uint)(g_buffer_heap.buffer_read<uint16>(heap_idx, hit.prim / 16u));
		return ((value >> (hit.prim & 15u)) & 1u) != 0;
	}

	auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
	auto mat_meta = material::mat_meta(g_buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, hit.prim);
	auto pos_uv = geometry::read_vert_pos_uv(g_buffer_heap, hit.prim, inst_info.mesh);
	float2 uv = interpolate(hit.bary, pos_uv[0].uv, pos_uv[1].uv, pos_uv[2].uv);

	vt::VTMeta vt_meta;
	vt_meta.frame_countdown = idxs.frame_countdown;

	return !material::cutout(g_buffer_heap, g_image_heap, mat_meta, uv, vt_meta, rng);
}
static bool commit_procedural(Ray ray, auto hit, auto& rng, float& hit_dist, ProceduralGeometry& geometry) {
	hit_dist = ray.t_max;
	return sampling::sample_procedural(ray, hit, rng, hit_dist, geometry);
}
#endif
static CommittedHit rbq_trace_closest(Ray ray, TraceIndices auto const& idxs, auto& rng, ProceduralGeometry& procedural_geometry, uint mask = max_uint32) {
#ifdef RBQ_USE_RAYQUERY
	auto query = g_accel.query_all(ray, mask);
	TriangleHit hit;
	ProceduralHit proc_hit;
	float dist = ray.t_max;
	while (query.proceed()) {
		if (query.is_triangle_candidate()) {
			hit = query.triangle_candidate();
			if (commit_triangle(hit, idxs, rng)) {
				query.commit_triangle();
			}
		} else {
			proc_hit = query.procedural_candidate();
			if (commit_procedural(query.world_ray(), proc_hit, rng, dist, procedural_geometry)) {
				query.commit_procedural(dist);
			}
		}
	}
	return query.committed_hit();
#else
	return g_accel.trace_closest(ray, mask);
#endif
}

static bool rbq_trace_any(Ray ray, TraceIndices auto const& idxs, auto& rng, uint mask = max_uint32) {
#ifdef RBQ_USE_RAYQUERY_SHADOW
	auto query = g_accel.query_any(ray, mask);
	TriangleHit hit;
	ProceduralHit proc_hit;
	float dist;
	while (query.proceed()) {
		if (query.is_triangle_candidate()) {
			hit = query.triangle_candidate();
			if (commit_triangle(hit, idxs, rng)) {
				return true;
			}
		} else {
			proc_hit = query.procedural_candidate();
			ProceduralGeometry geometry;
			if (commit_procedural(query.world_ray(), proc_hit, rng, dist, geometry)) {
				return true;
			}
		}
	}
	return !query.committed_hit().miss();
#else
	return g_accel.trace_any(ray, mask);
#endif
}