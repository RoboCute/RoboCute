#pragma once

#include <bsdfs/base/bsdf.hpp>
#include <geometry/vertices.hpp>
#include <material/procedural_mats.hpp>
#include <path_tracer/path.hpp>
#include <path_tracer/pt_args.hpp>
#include <path_tracer/medium_transition.hpp>
#include <sampling/sample_funcs.hpp>

namespace integrator {

class SurfaceInteraction {
public:
    material::MatMeta mat_meta;
    geometry::InstanceInfo inst_info;
    mtl::openpbr::BasicParameter basic_param;
    mtl::Onb vertices_onb;
    float4x4 inst_transform;
    std::array<float2, 4> uv;
    std::array<float3, 3> vertex_positions;
    std::array<float3, 3> vertex_normals;
    float3 world_pos;
    float3 vertices_normal;
    float3 plane_normal;
    float3 input_dir;
    float3 new_ray_offset = 0.0f;
    float3 emission = 0.0f;
    float3 albedo = 1e-1f;
#ifdef PT_MOTION_VECTORS
    float3 last_local_pos;
#endif
    float ray_t;
    float roughness;
    float eta = 1.0f;
    uint user_id;
    uint material_id;
    uint texture_filter;
    uint uv_count = 0u;
    mtl::BSDFFlags sample_flags = mtl::BSDFFlags::None;
    bool reject;
    bool is_indirect_ray;
    bool hit_triangle = true;
    bool contained_normal = false;
    bool contained_tangent = false;

    template<typename Hit, typename Procedural>
    void compute(
        pt::Path &state,
        Hit &hit,
        Procedural procedural_geometry,
        uint input_texture_filter,
        bool initial_reject) {
        vt::VTMeta vt_meta;
        vt_meta.frame_countdown = args.frame_countdown;
        texture_filter = input_texture_filter;
        reject = initial_reject;
        is_indirect_ray =
            state.detail != mtl::ShadingDetail::Default;
        user_id = g_accel.instance_user_id(hit.inst);

        if constexpr (requires { hit.hit_triangle(); }) {
            hit_triangle = hit.hit_triangle();
        }

        if (hit_triangle) {
            compute_triangle(
                state,
                hit,
                vt_meta);
        } else {
            compute_procedural(
                state,
                hit,
                procedural_geometry,
                state.ray.dir());
        }

        roughness = basic_param.specular.roughness *
                    (1.0f -
                     0.8f * basic_param.specular.roughness_anisotropy);
        albedo = float3(0.0f);
    }

    float3 to_local(float3 direction) const {
        return basic_param.geometry.onb.to_local(direction);
    }

    float3 to_world(float3 direction) const {
        return basic_param.geometry.onb.to_world(direction);
    }

    float3 shading_normal() const {
        return basic_param.geometry.onb.normal;
    }

    float4 texture_gradient() const {
        return fixed_texture_gradient(texture_filter);
    }

    Ray spawn_ray(float3 direction, float ray_t_min) const {
        auto offset_normal =
            dot(plane_normal, direction) < 0.0f ? -plane_normal : plane_normal;
        return Ray(
            new_ray_offset + sampling::offset_ray_origin(
                                 world_pos,
                                 offset_normal),
            direction,
            ray_t_min);
    }

    void set_bsdf_sample(mtl::BSDFSample const &sample) {
        sample_flags = sample.throughput.flags;
        eta = sample.eta;
    }

    bool relocate(auto &bsdf) {
        if constexpr (requires { bsdf.sampled_interaction(); }) {
            auto sampled = bsdf.sampled_interaction();
            if (static_cast<bool>(sampled)) {
                world_pos = sampled.position;
                plane_normal = sampled.normal;
                contained_normal = false;
                return true;
            }
        }
        return false;
    }

    float3 bend_outgoing(
        MediumTransition const &boundary,
        float3 direction) const {
        bool output_outside =
            boundary.entering == mtl::is_reflective(sample_flags);
        return mtl::bend_to_hemisphere(
            direction,
            output_outside ? vertices_normal : -vertices_normal);
    }

    void update_ray_offset(bool relocated) {
        if (!relocated &&
            mtl::is_transmissive(sample_flags) &&
            basic_param.geometry.thin_walled) {
            new_ray_offset =
                -basic_param.geometry.onb.normal *
                basic_param.geometry.thickness;
        }
    }

    template<typename Hit>
    MediumTransition medium_transition(
        pt::Path const &path,
        Hit const &hit) const {
        MediumTransition boundary;
        boundary.entering = dot(input_dir, plane_normal) < 0.0f;
        if (hit_triangle &&
            !basic_param.geometry.thin_walled &&
            (basic_param.weight.transmission > 0.0f ||
             basic_param.weight.subsurface > 0.0f)) {
            boundary.id = hit.inst;
            boundary.uses_subsurface =
                basic_param.weight.transmission <= 0.0f;
            mtl::Volume candidate;
            candidate.boundary_id = boundary.id;
            candidate.nested_priority =
                basic_param.geometry.nested_priority;
            boundary.is_false =
                !mtl::active_medium_boundary_is_true(
                    path.media,
                    candidate,
                    boundary.entering);
        }
        return boundary;
    }

    void exit_false_boundary(
        pt::Path &path,
        MediumTransition const &boundary) {
        mtl::active_medium_remove(path.media, boundary.id);
        sample_flags = mtl::BSDFFlags::NoMediumChange;
    }

    void update_medium(
        pt::Path &path,
        auto const &bsdf,
        MediumTransition const &boundary,
        mtl::BSDFSample const &sample,
        auto const &context) const {
        if (boundary.entering) {
            mtl::Volume medium;
            bool has_medium = bsdf.fill_medium(
                medium,
                mtl::is_diffuse(sample.throughput.flags));
            if (has_medium) {
                medium.ior =
                    context.specular_fresnel.ior() /
                    context.inv_out_ior;
                medium.boundary_id = boundary.id;
                medium.nested_priority =
                    basic_param.geometry.nested_priority;
                if (!mtl::active_medium_insert(path.media, medium)) {
                    path.active = false;
                    path.beta = 0.0f;
                }
            }
        } else {
            mtl::active_medium_remove(path.media, boundary.id);
        }
    }

private:
    static float4 fixed_texture_gradient(uint filter) {
        return filter == Filter::POINT ? float4(1.0f) : float4(0.0f);
    }

    template<typename Hit>
    void compute_triangle(
        pt::Path &state,
        Hit &hit,
        vt::VTMeta const &vt_meta) {
        inst_info =
            g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(
                heap_indices::inst_buffer_heap_idx,
                user_id);
        geometry::Triangle triangle;
        auto vertices = geometry::read_vertices(
            g_buffer_heap,
            hit.prim,
            inst_info.mesh,
            contained_normal,
            contained_tangent,
            uv_count,
            triangle);
        mat_meta = material::mat_meta(
            g_buffer_heap,
            heap_indices::mat_idx_buffer_heap_idx,
            inst_info.mesh.submesh_heap_idx,
            inst_info.mat_index,
            hit.prim);
        material_id = material::to_mat_code(mat_meta);
        inst_transform = g_accel.instance_transform(hit.inst);
        auto local_pos = hit.interpolate(
            vertices[0].pos,
            vertices[1].pos,
            vertices[2].pos);
#ifdef PT_MOTION_VECTORS
        last_local_pos = local_pos;
#endif
        for (int i = 0; i < 3; ++i) {
            vertex_positions[i] =
                (inst_transform * float4(vertices[i].pos, 1)).xyz;
            vertex_normals[i] = vertices[i].normal;
        }
        plane_normal = cross(
            vertex_positions[0] - vertex_positions[1],
            vertex_positions[0] - vertex_positions[2]);
        plane_normal = normalize(plane_normal);
        world_pos = (inst_transform * float4(local_pos, 1)).xyz;
        for (int i = 0; i < uv_count; ++i) {
            uv[i] = hit.interpolate(
                vertices[0].uvs[i],
                vertices[1].uvs[i],
                vertices[2].uvs[i]);
        }

        input_dir = world_pos - state.segment_origin;
        ray_t = length(input_dir);
        hit.ray_t = ray_t;
        input_dir = input_dir / ray_t;

        if (contained_normal) {
            vertices_normal = hit.interpolate(
                vertices[0].normal,
                vertices[1].normal,
                vertices[2].normal);
            vertices_normal = normalize(
                mtl::make_normal_transform(inst_transform) *
                vertices_normal);
            if (dot(input_dir, plane_normal) *
                    dot(input_dir, vertices_normal) <
                0.0f) {
                vertices_normal = plane_normal;
            }
        } else {
            vertices_normal = plane_normal;
        }

        if (contained_tangent) {
            auto tangent = hit.interpolate(
                vertices[0].tangent,
                vertices[1].tangent,
                vertices[2].tangent);
            basic_param.geometry.onb.tangent = normalize(
                                                   inst_transform *
                                                   float4(tangent.xyz, 0))
                                                   .xyz;
            basic_param.geometry.onb.bitangent = normalize(cross(
                                                     vertices_normal,
                                                     basic_param.geometry.onb.tangent)) *
                                                 tangent.w;
            basic_param.geometry.onb.normal = vertices_normal;
        } else {
            basic_param.geometry.onb = mtl::Onb(vertices_normal);
        }

        vertices_onb = basic_param.geometry.onb;
        if (dot(input_dir, basic_param.geometry.onb.normal) >= 0.0f) {
            basic_param.geometry.onb.normal =
                -basic_param.geometry.onb.normal;
            basic_param.geometry.onb.tangent =
                -basic_param.geometry.onb.tangent;
        }
        state.active = material::transform_to_params(
            g_buffer_heap,
            g_image_heap,
            mat_meta,
            basic_param,
            texture_filter,
            vt_meta,
            uv,
            uv_count,
            fixed_texture_gradient(texture_filter),
            input_dir,
            world_pos,
            reject);
    }

    template<typename Hit, typename Procedural>
    void compute_procedural(
        pt::Path &state,
        Hit &hit,
        Procedural procedural_geometry,
        float3 ray_direction) {
        plane_normal = float3(procedural_geometry.normal);
        vertices_normal = plane_normal;
        ray_t = hit.ray_t;
        world_pos =
            (ray_t - 1e-3f) * ray_direction + state.segment_origin +
            vertices_normal * 1e-4f;
        input_dir = normalize(world_pos - state.segment_origin);
        basic_param.geometry.onb = mtl::Onb(vertices_normal);
        vertices_onb = basic_param.geometry.onb;
        state.active = ray_t > 0;
        if (state.active) {
            state.active =
                material::procedural_transform_to_params(
                    g_buffer_heap,
                    g_image_heap,
                    procedural_geometry.procedural_id,
                    basic_param,
                    input_dir,
                    world_pos,
                    reject);
        }
    }
};

}// namespace integrator
