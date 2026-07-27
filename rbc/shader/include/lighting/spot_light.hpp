#pragma once

#include <lighting/light_sample.hpp>
#include <luisa/resources/common_extern.hpp>
#include <sampling/sample_funcs.hpp>
#include <spectrum/spectrum.hpp>
#include <utils/onb.hpp>

namespace lighting {

using namespace luisa::shader;

class SpotLight {
public:
    std::array<float, 3> radiance;
    float angle_radian;
    float4 sphere;
    std::array<float, 3> forward_dir;
    float small_angle_radian;
    float angle_atten_power;
    float mis_weight;

    float3 pos() const {
        return sphere.xyz;
    }

    void set_pos(float3 pos) {
        sphere.xyz = pos;
    }

    float radius() const {
        return sphere.w;
    }

    void set_radius(float radius) {
        sphere.w = radius;
    }

    static SpotLight read(uint light_idx) {
        return g_buffer_heap.uniform_idx_buffer_read<SpotLight>(
            heap_indices::spot_lights_heap_idx,
            light_idx);
    }

    void sample_direction(
        auto const &light_sampler,
        SpectrumArg &spectrum_arg,
        auto &sampler,
        LightSample &result) const {
        result.mis_weight = mis_weight;
        auto world_pos = light_sampler.world_pos();
        auto wi_light = pos() - world_pos;
        // triangle: longest edge is c, near angle is b, on angle's other side is a
        auto wi_length = length(wi_light);
        wi_light = wi_light / wi_length;
        auto d_light = wi_length;
        if (wi_length <= radius()) {
            return;
        }
        mtl::Onb to_light_onb(wi_light);
        float half_light_angle = asin(radius() / wi_length);
        float cos_theta_max = cos(half_light_angle);
        float3 cone_dir =
            sampling::uniform_sample_cone(sampler.next2f(), cos_theta_max);
        wi_light = normalize(to_light_onb.to_world(cone_dir));
        float angle = acos(dot(-wi_light, float3(forward_dir)));
        if (angle >= angle_radian) {
            return;
        }
        {
            auto t = detail::ray_intersect_sphere(
                wi_light * wi_length,
                world_pos,
                pos(),
                radius());
            if (t < 0) {
                return;
            }
            wi_length = wi_length * t;
        }
        result.wi = wi_light;
        result.pdf = 1.0f / (2 * pi * (1.0f - cos_theta_max));
        float3 light_radiance = float3(radiance);
        light_radiance = spectrum::emission_to_spectrum(
            g_image_heap,
            g_volume_heap,
            spectrum_arg,
            args.resource_to_rec2020_mat * light_radiance);
        result.wi_length = wi_length;
        angle = saturate(
            (angle - angle_radian) /
            (small_angle_radian - angle_radian));
        angle = pow(angle, angle_atten_power);
        result.L =
            light_radiance /
            max(
                1e-10f,
                light_sampler.selection_probability() *
                    (sqr(radius()) *
                         max(0.0f, wi_length - 0.5f) +
                     1.0f)) *
            angle;
    }

    static void eval_hit(
        uint light_idx,
        auto &surface,
        auto &path,
        float pdf_bsdf) {
        auto light = read(light_idx);
        auto wi_light = light.pos() - path.segment_origin;
        auto wi_length = length(wi_light);
        wi_light = wi_light / wi_length;
        if (wi_length > light.radius()) {
            float half_light_angle = asin(light.radius() / wi_length);
            float cos_theta_max = cos(half_light_angle);
            auto pdf_light = 1.0f / (2 * pi * (1.0f - cos_theta_max));
            auto mis = pdf_bsdf < 0.0f ? 1.0f : float(sampling::balanced_heuristic(pdf_bsdf, pdf_light));
            float angle = acos(dot(
                -surface.input_dir,
                float3(light.forward_dir)));
            angle = saturate(
                (angle - light.angle_radian) /
                (light.small_angle_radian - light.angle_radian));
            angle = pow(angle, light.angle_atten_power);
            surface.emission *= angle;
            surface.emission *= mis;
        }
        path.active = false;
    }
};

}// namespace lighting
