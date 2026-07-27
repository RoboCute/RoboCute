#pragma once

#include <material/procedural_mats.hpp>
#include <path_tracer/surface_interaction.hpp>
#include <spectrum/spectrum.hpp>

namespace integrator {

template<
    class MatBSDF,
    class MatExtraParameter,
    mtl::TransportMode transport_mode>
class BSDFInstance {
public:
    using Model = MatBSDF;
    using Context =
        mtl::BSDFContext<MatExtraParameter, transport_mode>;

    void load(
        pt::Path &path,
        auto procedural_geometry,
        SurfaceInteraction &surface) {
        initialize_spectrum(
            surface.basic_param,
            surface.basic_param,
            path.spectrum,
            args.resource_to_rec2020_mat);

        load_parameters(
            path,
            procedural_geometry,
            surface);
        initialize_spectrum(
            _extra_parameter,
            surface.basic_param,
            path.spectrum,
            args.resource_to_rec2020_mat);
    }

private:
    void load_parameters(
        pt::Path &path,
        auto procedural_geometry,
        SurfaceInteraction &surface) {
        vt::VTMeta vt_meta;
        vt_meta.frame_countdown = args.frame_countdown;

        if constexpr (requires { _extra_parameter.coat; }) {
            _extra_parameter.coat.coat_onb = surface.vertices_onb;
        }
        if (surface.hit_triangle) {
            material::transform_to_params(
                g_buffer_heap,
                g_image_heap,
                surface.mat_meta,
                _extra_parameter,
                surface.texture_filter,
                vt_meta,
                surface.uv,
                surface.uv_count,
                surface.texture_gradient(),
                surface.input_dir,
                surface.world_pos,
                surface.reject);
        } else {
            path.active =
                material::procedural_transform_to_params(
                    g_buffer_heap,
                    g_image_heap,
                    procedural_geometry.procedural_id,
                    _extra_parameter,
                    surface.input_dir,
                    surface.world_pos,
                    surface.reject);
        }
    }

public:
    float3 eval_emission(
        SurfaceInteraction const &surface) const {
        float3 emission = 0.0f;
        if (surface.basic_param.geometry.thin_walled ||
            dot(
                surface.input_dir,
                surface.basic_param.geometry.onb.normal) < 0.0f) {
            emission =
                surface.basic_param.emission.luminance.spectral();
            if constexpr (requires { _extra_parameter.coat; }) {
                emission *= lerp(
                    float3(1.0f),
                    _extra_parameter.coat.color.spectral(),
                    surface.basic_param.weight.coat);
            }
        }
        return emission;
    }

    Context make_context(
        pt::Path const &path,
        SurfaceInteraction &surface) {
        return Context{
            surface.basic_param,
            _extra_parameter,
            path.spectrum.lambda,
            path.detail};
    }

    void configure_context(
        Context &context,
        pt::Path const &path,
        SurfaceInteraction const &surface,
        MediumTransition const &boundary) const {
        context.entering = boundary.entering;
        if (surface.basic_param.geometry.thin_walled) {
            if (!path.media.empty()) {
                context.inv_out_ior = rcp(path.media.back().ior);
            }
        } else {
            context.inv_out_ior = rcp(
                mtl::active_medium_ior_across_boundary(
                    path.media,
                    boundary.id,
                    boundary.entering));
        }
        context.selected_wavelength =
            path.spectrum.selected_wavelength;
        context.hero_wavelength_index =
            path.spectrum.hero_index;
    }

    void initialize_context(
        Context &context,
        SurfaceInteraction const &surface,
        float3 random) const {
        context.rand = random;
        context.init(surface.basic_param, _extra_parameter);
    }

    void init(
        Context &context,
        pt::Path const &path,
        SurfaceInteraction &surface,
        float3 wi,
        bool need_albedo) {
        if constexpr (requires {
                          _model.prepare_interaction(
                              surface.world_pos,
                              path.segment_origin,
                              surface.hit_triangle && path.media.empty());
                      }) {
            _model.prepare_interaction(
                surface.world_pos,
                path.segment_origin,
                surface.hit_triangle && path.media.empty());
        }
        _model.init(
            wi,
            surface.basic_param,
            _extra_parameter,
            context);
        if (need_albedo) {
            bool old_flag = context.spectrumed;
            context.spectrumed = false;
            surface.albedo = _model.energy(wi, context);
            context.spectrumed = old_flag;
        }
    }

    mtl::BSDFSample sample(
        Context &context,
        mtl::ActiveMediumList &media,
        float3 wi,
        float3 random) {
        context.rand = random;
        return _model.sample(wi, context, media);
    }

    mtl::Throughput eval(
        Context &context,
        float3 wi,
        float3 wo) {
        return _model.eval(wi, wo, context);
    }

    float pdf(
        Context &context,
        float3 wi,
        float3 wo) {
        return _model.pdf(wi, wo, context);
    }

    float4 eval_pdf(
        Context &context,
        SurfaceInteraction const &surface,
        float3 wi,
        float3 light_direction) {
        float3 wo = surface.to_local(light_direction);
        auto old_flags = context.sampling_flags;
        if (dot(surface.vertices_normal, light_direction) *
                dot(surface.vertices_normal, surface.input_dir) <
            0.0f) {
            if (!mtl::is_reflective(surface.sample_flags)) {
                return float4(0.0f);
            }
            context.sampling_flags = mtl::BSDFFlags(
                old_flags & mtl::BSDFFlags::Reflection);
        } else {
            if (!mtl::is_transmissive(surface.sample_flags)) {
                return float4(0.0f);
            }
            context.sampling_flags = mtl::BSDFFlags(
                old_flags & mtl::BSDFFlags::Transmission);
        }
        auto value = eval(context, wi, wo);
        auto density = pdf(context, wi, wo);
        context.sampling_flags = old_flags;
        if (!value ||
            any(value.val < 0.0f) ||
            !all(is_finite(value.val)) ||
            !is_finite(density) ||
            density <= 0.0f) {
            return float4(0.0f);
        }
        return float4(value.val, density);
    }

    bool relocate(SurfaceInteraction &surface) {
        return surface.relocate(_model);
    }

private:
    void initialize_spectrum(
        auto &parameter,
        mtl::openpbr::BasicParameter const &basic_parameter,
        SpectrumArg &spectrum_arg,
        float3x3 resource_to_rec2020_mat) {
        if constexpr (requires { parameter.emission; }) {
            parameter.emission.luminance.init_emission(
                g_image_heap,
                g_volume_heap,
                spectrum_arg,
                resource_to_rec2020_mat);
        }
        if constexpr (requires { parameter.base; }) {
            parameter.base.color.init_reflectance(
                g_volume_heap,
                spectrum_arg,
                resource_to_rec2020_mat);
        }
        if constexpr (requires { parameter.specular; }) {
            parameter.specular.color.init_reflectance(
                g_volume_heap,
                spectrum_arg,
                resource_to_rec2020_mat);
        }
        if constexpr (requires { parameter.diffraction; }) {
            if (basic_parameter.weight.diffraction > 0.0f) {
                parameter.diffraction.color.init_reflectance(
                    g_volume_heap,
                    spectrum_arg,
                    resource_to_rec2020_mat);
            }
        }
        if constexpr (requires { parameter.coat; }) {
            if (basic_parameter.weight.coat > 0.0f) {
                parameter.coat.color.init_reflectance(
                    g_volume_heap,
                    spectrum_arg,
                    resource_to_rec2020_mat);
            }
        }
        if constexpr (requires { parameter.fuzz; }) {
            if (basic_parameter.weight.fuzz > 0.0f) {
                parameter.fuzz.color.init_reflectance(
                    g_volume_heap,
                    spectrum_arg,
                    resource_to_rec2020_mat);
            }
        }
        if constexpr (requires { parameter.subsurface; }) {
            if (basic_parameter.weight.subsurface > 0.0f) {
                parameter.subsurface.color.init_reflectance(
                    g_volume_heap,
                    spectrum_arg,
                    resource_to_rec2020_mat);
                parameter.subsurface.radius =
                    spectrum::reflectance_to_spectrum(
                        g_volume_heap,
                        spectrum_arg,
                        parameter.subsurface.radius);
            }
        }
        if constexpr (requires { parameter.transmission; }) {
            if (basic_parameter.weight.transmission > 0.0f) {
                parameter.transmission.color.init_reflectance(
                    g_volume_heap,
                    spectrum_arg,
                    resource_to_rec2020_mat);
                if (any(parameter.transmission.scatter != 0.0f)) {
                    parameter.transmission.scatter =
                        spectrum::reflectance_to_spectrum(
                            g_volume_heap,
                            spectrum_arg,
                            parameter.transmission.scatter);
                }
            }
        }
    }

public:
    bool select_medium_wavelength(
        mtl::Volume &medium,
        SpectrumArg &spectrum_arg,
        bool diffuse,
        float ior) const {
        medium.ior = ior;
        bool selected_wavelength_now = false;
        if (!diffuse) {
            if constexpr (requires { _extra_parameter.transmission; }) {
                if (_extra_parameter.transmission.dispersion_scale > 0.0f) {
                    selected_wavelength_now =
                        !spectrum_arg.selected_wavelength;
                    spectrum_arg.selected_wavelength = true;
                    float hero_lambda =
                        spectrum_arg.lambda[spectrum_arg.hero_index];
                    spectrum_arg.lambda = hero_lambda;
                    medium.ior = mtl::dispersion_ior(
                        ior,
                        _extra_parameter.transmission
                            .dispersion_abbe_number,
                        _extra_parameter.transmission.dispersion_scale,
                        hero_lambda);
                }
            }
        }
        return selected_wavelength_now;
    }

    bool fill_medium(
        mtl::Volume &medium,
        bool diffuse) const {
        bool has_medium = false;
        if (diffuse) {
            if constexpr (requires { _extra_parameter.subsurface; }) {
                medium.fill_from_subsurface(
                    _extra_parameter.subsurface);
                has_medium = true;
            }
        } else {
            if constexpr (requires { _extra_parameter.transmission; }) {
                medium.fill_from_transmission(
                    _extra_parameter.transmission);
                has_medium = true;
            }
        }
        return has_medium;
    }

private:
    MatBSDF _model;
    MatExtraParameter _extra_parameter;
};

}// namespace integrator
