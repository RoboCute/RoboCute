#pragma once

#include <lighting/light_sample.hpp>
#include <luisa/resources/common_extern.hpp>
#include <sampling/sample_funcs.hpp>
#include <spectrum/spectrum.hpp>

namespace lighting {

using namespace luisa::shader;

class AreaLight {
public:
    float4x4 transform;
    std::array<float, 3> radiance;
    float area;
    uint emission;
    float mis_weight;

    static AreaLight read(uint light_idx) {
        return g_buffer_heap.uniform_idx_buffer_read<AreaLight>(
            heap_indices::area_lights_heap_idx,
            light_idx);
    }

    void sample_direction(
        auto const &light_sampler,
        SpectrumArg &spectrum_arg,
        auto &sampler,
        LightSample &result) const {
        result.mis_weight = mis_weight;
        auto world_pos = light_sampler.world_pos();
        auto light_uv = sampler.next2f();
        auto light_local_pos = light_uv - 0.5f;
        float4x4 light_transform = transform;
        auto p_light =
            (light_transform * float4(light_local_pos, 0.f, 1.f)).xyz;
        auto light_normal = (light_transform * float4(0, 0, 1, 0)).xyz;
        light_normal = normalize(light_normal);
        auto pp_light = p_light + light_normal * 1e-5f;
        auto wi_light = pp_light - world_pos;
        auto d_light = length(wi_light);
        wi_light = wi_light / d_light;
        result.wi = wi_light;
        auto cos_light = -dot(light_normal, wi_light);
        result.pdf =
            (d_light * d_light) /
            max(area * cos_light, 1e-5f);
        result.wi_length = d_light;
        auto light_emission = float3(radiance);
        if (emission != max_uint32) {
            light_emission *= g_image_heap.image_sample(
                                              emission,
                                              light_uv,
                                              Filter::LINEAR_POINT,
                                              Address::REPEAT)
                                  .xyz;
        }
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
        auto &path,
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
        float light_area = sqrt(p * (p - a) * (p - b) * (p - c)) * 2.f;
        auto pdf_light =
            (surface.ray_t * surface.ray_t) /
            max(
                light_area * max(
                                 dot(
                                     -surface.input_dir,
                                     surface.vertices_normal),
                                 0.f),
                1e-5f);
        auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
        surface.emission *= mis;
        path.active = false;
    }
};

}// namespace lighting
