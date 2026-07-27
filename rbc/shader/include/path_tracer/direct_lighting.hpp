#pragma once

#include <lighting/light_sampler.hpp>
#include <path_tracer/environment_light.hpp>
#include <path_tracer/surface_interaction.hpp>
#include <sampling/sample_funcs.hpp>
#include <spectrum/spectrum.hpp>

namespace integrator {

struct DirectLightSample {
    float4 radiance;
    float mis;
    uint light_type;
    uint light_index;
};

class DirectLighting {
public:
    DirectLighting(
        float3 shading_normal,
        float3 wo,
        float roughness,
        bool use_specular,
        bool is_primary_ray)
        : _shading_normal(shading_normal),
          _wo(wo),
          _roughness(roughness),
          _use_specular(use_specular),
          _is_primary_ray(is_primary_ray) {}

    DirectLightSample sample(
        SpectrumArg &spectrum_arg,
        auto &bsdf,
        auto &context,
        float3 wi,
        auto &sampler,
        SurfaceInteraction const &surface,
        auto hit) const {
        auto bsdf_eval = [
                             &bsdf,
                             &context,
                             &surface,
                             wi](float3 light_direction) {
            return bsdf.eval_pdf(
                context,
                surface,
                wi,
                light_direction);
        };
        if (surface.contained_normal) {
            return sample_with_shadow_terminator_fix(
                spectrum_arg,
                bsdf_eval,
                sampler,
                surface,
                hit);
        }
        return sample_from_surface(
            spectrum_arg,
            bsdf_eval,
            sampler,
            surface);
    }

private:
    DirectLightSample sample_with_shadow_terminator_fix(
        SpectrumArg &spectrum_arg,
        auto &bsdf_eval,
        auto &sampler,
        SurfaceInteraction const &surface,
        auto hit) const {
        bool need_flip = false;
        float3 offset_normal = surface.plane_normal;
        float3 direct_normal = surface.plane_normal;

        if (dot(_wo, offset_normal) < 0.0f) {
            offset_normal = -offset_normal;
            need_flip = true;
        }
        if (dot(_wo, direct_normal) < 0.0f) {
            direct_normal = -direct_normal;
        }

        float3 direct_position = surface.world_pos;
        float3x3 normal_transform =
            mtl::make_normal_transform(surface.inst_transform);
        float3 n0 = normalize(normal_transform * surface.vertex_normals[0]);
        float3 n1 = normalize(normal_transform * surface.vertex_normals[1]);
        float3 n2 = normalize(normal_transform * surface.vertex_normals[2]);
        if (need_flip) {
            n0 = -n0;
            n1 = -n1;
            n2 = -n2;
        }
        float3 d0 = direct_position - surface.vertex_positions[0];
        float3 d1 = direct_position - surface.vertex_positions[1];
        float3 d2 = direct_position - surface.vertex_positions[2];
        d0 -= min(0.0f, dot(d0, n0)) * n0;
        d1 -= min(0.0f, dot(d1, n1)) * n1;
        d2 -= min(0.0f, dot(d2, n2)) * n2;
        direct_position += hit.interpolate(d0, d1, d2);
        direct_position = sampling::offset_ray_origin(
            direct_position,
            offset_normal);

        return sample_impl(
            spectrum_arg,
            bsdf_eval,
            sampler,
            direct_position,
            direct_normal);
    }

    DirectLightSample sample_from_surface(
        SpectrumArg &spectrum_arg,
        auto &bsdf_eval,
        auto &sampler,
        SurfaceInteraction const &surface) const {
        float3 offset_normal = surface.plane_normal;
        float3 direct_normal = _shading_normal;
        if (dot(_wo, offset_normal) < 0.0f) {
            offset_normal = -offset_normal;
        }
        if (dot(_wo, direct_normal) < 0.0f) {
            direct_normal = -direct_normal;
        }
        float3 direct_position = sampling::offset_ray_origin(
            surface.world_pos,
            offset_normal);

        return sample_impl(
            spectrum_arg,
            bsdf_eval,
            sampler,
            direct_position,
            direct_normal);
    }

    DirectLightSample sample_impl(
        SpectrumArg &spectrum_arg,
        auto &bsdf_eval,
        auto &sampler,
        float3 position,
        float3 normal) const {
        DirectLightSample result;
        pt::EnvironmentLight environment;
        auto hdri_sample = environment.sample_direction(
            spectrum_arg,
            sampler.next2f());

        lighting::LightSampler light_sampler(
            position,
            normal,
            _wo,
            _roughness,
            _use_specular,
            args.light_count);
        auto di_sample = light_sampler.sample_direction(
            spectrum_arg,
            sampler);
        result.light_type = light_sampler.light_type();
        result.light_index = light_sampler.light_index();
        float select_weight = 1.f;
        float hdri_lum;
        float di_lum;
        float rand = sampler.next();
        const float epsilon = 1e-5f;
        if (_is_primary_ray) {
            if (hdri_sample.pdf > epsilon) {
                auto eval_result = bsdf_eval(hdri_sample.wi);
                if (eval_result.w > epsilon &&
                    !rbc_trace_any(
                        Ray(
                            position,
                            hdri_sample.wi,
                            sampling::offset_ray_t_min,
                            1e8f),
                        args,
                        sampler,
                        ONLY_OPAQUE_MASK)) {
                    float hdri_mis = sampling::balanced_heuristic(
                        hdri_sample.pdf,
                        eval_result.w);
                    result.radiance.xyz =
                        hdri_sample.L * hdri_mis * eval_result.xyz /
                        max(epsilon, hdri_sample.pdf);
                }
                result.radiance.w = 1024.f;
            }
        } else {
            hdri_lum =
                reduce_sum(hdri_sample.L) /
                max(hdri_sample.pdf, epsilon) *
                sqrt(abs(dot(hdri_sample.wi, normal)));
            di_lum =
                reduce_sum(di_sample.L) /
                max(di_sample.pdf, epsilon) *
                sqrt(abs(dot(di_sample.wi, normal)));
            select_weight = di_lum + hdri_lum;
            if (select_weight <= epsilon) {
                return result;
            }
            select_weight = di_lum / select_weight;
            if (rand < select_weight) {
                result.radiance.w = di_sample.wi_length;
            } else {
                di_sample.mis_weight = 1.f;
                di_sample.L = hdri_sample.L;
                di_sample.pdf = hdri_sample.pdf;
                di_sample.wi = hdri_sample.wi;
                di_sample.wi_length = 1e8f;
                result.radiance.w = 1024.f;
                result.light_type = max_uint32;
                result.light_index = max_uint32;
                select_weight = 1.f - select_weight;
            }
        }
        if (di_sample.pdf < epsilon) {
            result.light_type = max_uint32;
            result.light_index = max_uint32;
            return result;
        }
        auto eval_result = bsdf_eval(di_sample.wi);
        if (eval_result.w < epsilon ||
            rbc_trace_any(
                Ray(
                    position,
                    di_sample.wi,
                    sampling::offset_ray_t_min,
                    max(
                        sampling::offset_ray_t_min,
                        di_sample.wi_length -
                            sampling::offset_ray_t_min)),
                args,
                sampler,
                ONLY_OPAQUE_MASK)) {
            result.light_type = max_uint32;
            result.light_index = max_uint32;
            return result;
        }
        result.mis = sampling::weighted_balanced_heuristic(
            di_sample.mis_weight,
            di_sample.pdf,
            eval_result.w);
        float3 di_result =
            di_sample.L * result.mis * eval_result.xyz /
            max(epsilon, di_sample.pdf);
        if (_is_primary_ray) {
            hdri_lum = dot(result.radiance.xyz, float3(1.f)) /
                       max(hdri_sample.pdf, epsilon);
            di_lum = dot(di_result, float3(1.f)) /
                     max(di_sample.pdf, epsilon);
            result.radiance.w = ite(
                rand < (hdri_lum /
                        max(di_lum + hdri_lum, epsilon)),
                1024.f,
                di_sample.wi_length);
        }
        result.radiance.xyz +=
            di_result / max(epsilon, select_weight);
        return result;
    }

private:
    float3 _shading_normal;
    float3 _wo;
    float _roughness;
    bool _use_specular;
    bool _is_primary_ray;
};

}// namespace integrator
