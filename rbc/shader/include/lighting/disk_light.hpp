#pragma once

#include <lighting/light_sample.hpp>
#include <luisa/resources/common_extern.hpp>
#include <sampling/sample_funcs.hpp>
#include <spectrum/spectrum.hpp>
#include <utils/onb.hpp>

namespace lighting {

using namespace luisa::shader;

class DiskLight {
public:
    std::array<float, 3> forward_dir;
    float area;
    std::array<float, 3> radiance;
    std::array<float, 3> position;
    float mis_weight;

    static DiskLight read(uint light_idx) {
        return g_buffer_heap.uniform_idx_buffer_read<DiskLight>(
            heap_indices::disk_lights_heap_idx,
            light_idx);
    }

    void sample_direction(
        auto const &light_sampler,
        SpectrumArg &spectrum_arg,
        auto &sampler,
        LightSample &result) const {
        result.mis_weight = mis_weight;
        auto world_pos = light_sampler.world_pos();
        mtl::Onb onb{float3(forward_dir)};
        auto p_light =
            float3(position) +
            onb.to_world(
                float3(sampling::sample_uniform_disk(sampler.next2f()), 0.f));
        auto light_normal = float3(forward_dir);
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
        uint light_idx,
        auto &surface,
        auto &path,
        float pdf_bsdf) {
        auto light = read(light_idx);
        auto pdf_light =
            (surface.ray_t * surface.ray_t) /
            max(
                light.area * max(
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
