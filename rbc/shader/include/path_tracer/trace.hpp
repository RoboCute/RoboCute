#pragma once
#include <luisa/std.hpp>
#include <luisa/resources/common_extern.hpp>
#include <geometry/types.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <virtual_tex/stream.hpp>
#include <std/concepts>
#include <sampling/procedural_common.hpp>
namespace luisa::shader {
// #define RBC_USE_RAYQUERY
// #define RBC_USE_RAYQUERY_SHADOW

#if defined(RBC_USE_RAYQUERY) || defined(RBC_USE_RAYQUERY_SHADOW)
extern Buffer<uint> &g_triangle_vis_buffer;
#endif
extern Accel &g_accel;
#define DEFINED_G_ACCEL
}// namespace luisa::shader

#if defined(RBC_USE_RAYQUERY) || defined(RBC_USE_RAYQUERY_SHADOW)
#include <sampling/procedural_sampling.hpp>
#endif

using namespace luisa::shader;

template<class T>
concept TraceIndices = requires(T t) {
    std::is_same_v<decltype(t.frame_countdown), uint>;
};

#if defined(RBC_USE_RAYQUERY) || defined(RBC_USE_RAYQUERY_SHADOW)
static bool commit_triangle(TriangleHit hit, TraceIndices auto const &idxs, auto &rng) {
    auto user_id = g_accel.instance_user_id(hit.inst);
    auto heap_idx = g_triangle_vis_buffer.read(user_id);
    if (heap_idx != max_uint32) {
        auto value = (uint)(g_buffer_heap.buffer_read<uint16>(heap_idx, hit.prim / 16u));
        return ((value >> (hit.prim & 15u)) & 1u) != 0;
    }

    auto inst_info = g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(heap_indices::inst_buffer_heap_idx, user_id);
    auto mat_meta = material::mat_meta(g_buffer_heap, heap_indices::mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, hit.prim);
    uint uv_count;
    auto pos_uv = geometry::read_vert_pos_uv(g_buffer_heap, hit.prim, inst_info.mesh, uv_count);
    std::array<float2, 4> uv;
    for (uint i = 0; i < uv_count; ++i) {
        uv[i] = interpolate(hit.bary, pos_uv[0].uv[i], pos_uv[1].uv[i], pos_uv[2].uv[i]);
    }

    vt::VTMeta vt_meta;
    vt_meta.frame_countdown = idxs.frame_countdown;

    return !material::cutout(g_buffer_heap, g_image_heap, mat_meta, uv, uv_count, vt_meta, rng);
}
static bool commit_procedural(Ray ray, auto hit, auto &rng, float &hit_dist, ProceduralGeometry &geometry) {
    hit_dist = ray.t_max;
    return sampling::sample_procedural(ray, hit, rng, hit_dist, geometry);
}
#endif
static CommittedHit rbc_trace_closest(Ray ray, TraceIndices auto const &idxs, auto &rng, ProceduralGeometry &procedural_geometry, uint mask = max_uint32) {
#ifdef RBC_USE_RAYQUERY
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
            auto world_ray = query.world_ray();
            if (commit_procedural(world_ray, proc_hit, rng, dist, procedural_geometry)) {
                query.commit_procedural(dist);
            }
        }
    }
    return query.committed_hit();
#else
    auto hit = g_accel.trace_closest(ray, mask);
    CommittedHit r;
    r.inst = hit.inst;
    r.prim = hit.prim;
    r.bary = hit.bary;
    r.hit_type = hit.miss() ? HitTypes::Miss : HitTypes::HitTriangle;
    return r;
#endif
}

static bool rbc_trace_any(Ray ray, TraceIndices auto const &idxs, auto &rng, uint mask = max_uint32) {
#ifdef RBC_USE_RAYQUERY_SHADOW
    auto query = g_accel.query_any(ray, mask);
    TriangleHit hit;
    ProceduralHit proc_hit;
    float dist;
    while (query.proceed()) {
        device_log("proceed");
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