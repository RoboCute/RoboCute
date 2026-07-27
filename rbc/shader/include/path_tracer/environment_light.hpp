#pragma once

#include <luisa/resources/common_extern.hpp>
#include <path_tracer/pt_args.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/sample_hdri.hpp>
#include <spectrum/spectrum.hpp>

using namespace luisa::shader;

namespace pt {

class EnvironmentLight {
public:
    EnvironmentLight() : _sky_heap_idx(args.sky_heap_idx) {}

    HDRISample sample_direction(
        SpectrumArg &spectrum_arg,
        float2 random) const {
        if (_sky_heap_idx == max_uint32) {
            HDRISample sample;
            return sample;
        }
        auto sample = sample_hdri(
            args.world_2_sky_mat,
            _sky_heap_idx,
            args.alias_table_idx,
            args.pdf_table_idx,
            random);
        sample.L = spectrum::emission_to_spectrum(
            g_image_heap,
            g_volume_heap,
            spectrum_arg,
            args.resource_to_rec2020_mat * sample.L);
        return sample;
    }

    float3 eval_uv(SpectrumArg &spectrum_arg, float2 uv) const {
        auto sky = g_image_heap.uniform_idx_image_sample(
                                   _sky_heap_idx,
                                   uv,
                                   Filter::LINEAR_POINT,
                                   Address::EDGE)
                       .xyz;
        sky = args.resource_to_rec2020_mat * sky;
        return spectrum::emission_to_spectrum(
            g_image_heap,
            g_volume_heap,
            spectrum_arg,
            sky);
    }

    float3 eval(SpectrumArg &spectrum_arg, float3 direction) const {
        if (_sky_heap_idx == max_uint32) {
            return float3(0.0f);
        }
        auto uv = sampling::sphere_direction_to_uv(
            args.world_2_sky_mat,
            direction);
        return eval_uv(spectrum_arg, uv);
    }

    float3 eval_escaped_ray(
        SpectrumArg &spectrum_arg,
        float pdf_bsdf,
        float3 beta,
        float3 direction) const {
        if (_sky_heap_idx == max_uint32) return float3(0.0f);

        auto uv = sampling::sphere_direction_to_uv(
            args.world_2_sky_mat,
            direction);
        float theta;
        float phi;
        float3 wi;
        sampling::sphere_uv_to_direction_theta(
            args.world_2_sky_mat,
            uv,
            theta,
            phi,
            wi);

        float mis = 1.0f;
        if (pdf_bsdf > 0.0f) {
            auto tex_size = g_image_heap.uniform_idx_image_size(
                _sky_heap_idx);
            auto sky_coord = int2(uv * float2(tex_size));
            sky_coord = clamp(
                sky_coord,
                int2(0),
                int2(tex_size) - 1);
            auto sky_pdf = g_buffer_heap.uniform_idx_buffer_read<float>(
                args.pdf_table_idx,
                sky_coord.y * tex_size.x + sky_coord.x);
            sky_pdf = _directional_pdf(sky_pdf, theta);
            mis = sampling::balanced_heuristic(pdf_bsdf, sky_pdf);
        }
        return beta * mis * eval_uv(spectrum_arg, uv);
    }

    void accumulate_escaped_ray(
        SpectrumArg &spectrum_arg,
        float pdf_bsdf,
        float3 beta,
        float3 direction,
        float3 &radiance) const {
        radiance += eval_escaped_ray(
            spectrum_arg,
            pdf_bsdf,
            beta,
            direction);
    }

private:
    uint _sky_heap_idx;
};

}// namespace pt
