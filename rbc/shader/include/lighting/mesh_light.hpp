#pragma once

#include <geometry/vertices.hpp>
#include <lighting/light_sample.hpp>
#include <luisa/resources/common_extern.hpp>
#include <material/mats.hpp>
#include <sampling/sample_funcs.hpp>
#include <spectrum/spectrum.hpp>

namespace lighting {

using namespace luisa::shader;

class MeshLight {
public:
    float4x4 transform;
    std::array<float, 3> bounding_min;
    uint blas_heap_idx;
    std::array<float, 3> bounding_max;
    uint instance_user_id;
    float lum;
    float mis_weight;

    static MeshLight read(uint tlas_idx) {
        return g_buffer_heap.buffer_read<MeshLight>(
            heap_indices::mesh_lights_heap_idx,
            tlas_idx);
    }

    void sample_direction(
        auto &light_sampler,
        SpectrumArg &spectrum_arg,
        auto &sampler,
        LightSample &result) {
        if (!light_sampler.select_mesh_primitive(*this, sampler)) {
            return;
        }
        auto world_pos = light_sampler.world_pos();
        auto primitive_index = light_sampler.primitive_index();
        auto inst_info =
            g_buffer_heap.uniform_idx_buffer_read<geometry::InstanceInfo>(
                heap_indices::inst_buffer_heap_idx,
                instance_user_id);
        result.mis_weight = mis_weight;
        uint uv_count;
        auto vertices = geometry::read_vert_pos_uv(
            g_buffer_heap,
            primitive_index,
            inst_info.mesh,
            uv_count);
        float2 rand = sampler.next2f();
        if (rand.x + rand.y > 1) {
            rand = 1.f - rand;
        }
        float3 light_uvw(rand, 1.f - rand.x - rand.y);
        for (uint vv = 0; vv < 3; ++vv) {
            vertices[vv].pos =
                (transform * float4(vertices[vv].pos, 1.)).xyz;
        }
        auto p_light =
            vertices[0].pos * light_uvw.x +
            vertices[1].pos * light_uvw.y +
            vertices[2].pos * light_uvw.z;
        float3 light_normal;
        light_normal = cross(
            vertices[1].pos - vertices[0].pos,
            vertices[2].pos - vertices[0].pos);
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
        auto light_area = sqrt(
            halen_p *
            (halen_p - edge0) *
            (halen_p - edge1) *
            (halen_p - edge2));
        result.wi_length = d_light;
        result.pdf = (d_light * d_light) / (light_area * cos_light);
        result.wi = wi_light;
        std::array<float2, 4> uv;
        for (uint i = 0; i < uv_count; ++i) {
            uv[i] =
                vertices[0].uv[i] * light_uvw.x +
                vertices[1].uv[i] * light_uvw.y +
                vertices[2].uv[i] * light_uvw.z;
        }
        float3 light_emission = material::get_light_emission(
            g_buffer_heap,
            g_image_heap,
            heap_indices::mat_idx_buffer_heap_idx,
            inst_info.mesh.submesh_heap_idx,
            inst_info.mat_index,
            primitive_index,
            uv,
            uv_count);
        // auto col = mis_weight * light_emission / float(max(pdf_light, 1e-5f));
        light_emission = spectrum::emission_to_spectrum(
            g_image_heap,
            g_volume_heap,
            spectrum_arg,
            args.resource_to_rec2020_mat * light_emission);
        result.L = light_emission /
                   max(
                       1e-5f,
                       light_sampler.selection_probability());
    }

    static void eval_hit(
        uint,
        auto &surface,
        auto &,
        float pdf_bsdf) {
        float a = distance(
            surface.vertex_positions[0],
            surface.vertex_positions[1]);
        float b = distance(
            surface.vertex_positions[0],
            surface.vertex_positions[2]);
        float c = distance(
            surface.vertex_positions[1],
            surface.vertex_positions[2]);
        float p = (a + b + c) / 2.0f;
        float area = sqrt(p * (p - a) * (p - b) * (p - c));
        auto pdf_light =
            (surface.ray_t * surface.ray_t) /
            max(
                area * max(
                           dot(
                               -surface.input_dir,
                               surface.vertices_normal),
                           0.f),
                1e-5f);
        auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
        surface.emission *= mis;
    }
};

}// namespace lighting
