#pragma once

#include <bsdfs/base/sample.hpp>
#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>
#include <volumetric/medium_probe.hpp>
#include <volumetric/trace.hpp>
#include <volumetric/volume.hpp>

using namespace luisa::shader;

namespace pt {

class Path {
public:
    Ray ray;
    float3 segment_origin;
    float3 beta;
    float prev_bsdf_pdf;
    SpectrumArg spectrum;
    mtl::ActiveMediumList media;
    mtl::ShadingDetail detail;
    uint medium_boundary_steps;
    bool active;

    CommittedHit trace(
        auto &sampler,
        ProceduralGeometry &procedural_geometry,
        uint mask = max_uint32) {
        auto trace_origin = ray.origin();
        auto hit = mtl::trace_volumetric(
            media,
            beta,
            detail,
            ray,
            args,
            sampler,
            procedural_geometry,
            mask);
        if (any(ray.origin() != trace_origin)) {
            segment_origin = ray.origin();
            prev_bsdf_pdf = -1.0f;
        }
        return hit;
    }

    void spawn(
        auto const &interaction,
        float3 direction,
        float ray_t_min) {
        ray = interaction.spawn_ray(direction, ray_t_min);
    }

    void begin_segment() {
        segment_origin = ray.origin();
    }

    bool update_wavelength(auto const &context) {
        bool selected_now =
            !spectrum.selected_wavelength &&
            context.selected_wavelength;
        spectrum.selected_wavelength = context.selected_wavelength;
        if (selected_now) {
            spectrum.lambda = spectrum.lambda[spectrum.hero_index];
        }
        return selected_now;
    }

    bool accept(mtl::BSDFSample const &sample) {
        if (!sample ||
            any(sample.throughput.val < 0.0f) ||
            !all(is_finite(sample.throughput.val)) ||
            !is_finite(sample.pdf)) {
            active = false;
            beta = 0.0f;
            return false;
        }
        return true;
    }

    void collapse_wavelengths(bool selected_now) {
        if (selected_now) {
            mtl::collapse_active_medium_wavelengths(
                media,
                spectrum.hero_index);
        }
    }

    void update_throughput(
        mtl::BSDFSample const &sample,
        float &previous_pdf) {
        previous_pdf = max(1e-4f, sample.pdf);
        beta *= sample.throughput.val / previous_pdf;
        if (mtl::is_delta(sample.throughput.flags)) {
            previous_pdf = -1.0f;
        }
    }

    bool update_medium_boundary(bool no_medium_change) {
        if (no_medium_change) {
            return ++medium_boundary_steps >=
                   mtl::MAX_MEDIUM_BOUNDARY_STEPS;
        }
        medium_boundary_steps = 0u;
        return false;
    }

    void update_detail(
        mtl::BSDFFlags flags,
        bool relocated) {
        if (relocated) {
            detail = max(
                detail,
                mtl::ShadingDetail::IndirectSpecular);
        } else if (
            mtl::is_non_delta(flags) &&
            flags != mtl::BSDFFlags::SpecularTransmission) {
            detail = max(
                detail,
                mtl::is_specular(flags)
                    ? mtl::ShadingDetail::IndirectSpecular
                    : mtl::ShadingDetail::IndirectDiffuse);
        }
    }

};

}// namespace pt
