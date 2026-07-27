#pragma once

#include <bsdfs/polymorphic.hpp>
#include <lighting/polymorphic.hpp>
#include <path_tracer/bsdf_instance.hpp>
#include <path_tracer/direct_lighting.hpp>
#include <path_tracer/surface_interaction.hpp>
#include <spectrum/spectrum.hpp>

namespace integrator {

class PathVertex {
public:
    SurfaceInteraction si;

    template<
        mtl::TransportMode transport_mode,
        bool sample_lights>
    void sample(
        pt::Path &path,
        auto &sampler,
        auto &hit,
        auto procedural_geometry,
        float u_lobe,
        bool sample_bsdf,
        bool need_albedo,
        uint texture_filter,
        bool reject) {
        begin(path);
        si.compute(
            path,
            hit,
            procedural_geometry,
            texture_filter,
            reject);
        auto boundary = si.medium_transition(path, hit);

        if (boundary.is_false && !boundary.entering) {
            si.exit_false_boundary(path, boundary);
            _direct_radiance = 0.0f;
            _outgoing_direction = si.input_dir;
            _can_spawn = true;
        } else {
            visit_bsdf<transport_mode, sample_lights>(
                path,
                sampler,
                hit,
                procedural_geometry,
                boundary,
                u_lobe,
                sample_bsdf,
                need_albedo);
        }

        path.prev_bsdf_pdf = _outgoing_bsdf_pdf;
    }

    void apply_rejection(pt::Path &path) {
        si.reject &= args.require_reject;
        path.active &= !si.reject;
    }

    void apply_wavelength_transition(pt::Path &path) {
        if (!_selected_wavelength &&
            path.spectrum.selected_wavelength) {
            spectrum::modify_throughput(
                g_image_heap,
                path.spectrum,
                args.spectrum,
                path.beta,
                _throughput,
                _direct_radiance);
        }
    }

    bool advance(
        pt::Path &path,
        float surface_ray_t_min,
        bool continue_from_surface) const {
        if (!_can_spawn || !path.active) return false;

        bool null_boundary = no_medium_change();
        path.spawn(
            si,
            _outgoing_direction,
            null_boundary
                ? sampling::offset_ray_t_min
                : surface_ray_t_min);
        if (null_boundary) return true;
        if (!continue_from_surface) return false;
        path.begin_segment();
        return true;
    }

    bool no_medium_change() const {
        return mtl::is_no_medium_change(si.sample_flags);
    }

    float3 throughput() const { return _throughput; }
    float3 direct_radiance() const { return _direct_radiance; }
    float3 outgoing_direction() const { return _outgoing_direction; }

private:
    void begin(pt::Path const &path) {
        _throughput = path.beta;
        _outgoing_direction = path.ray.dir();
        _incoming_bsdf_pdf = path.prev_bsdf_pdf;
        _outgoing_bsdf_pdf = path.prev_bsdf_pdf;
        _can_spawn = false;
        _relocated = false;
        _selected_wavelength =
            path.spectrum.selected_wavelength;
    }

    template<
        mtl::TransportMode transport_mode,
        bool sample_lights>
    void visit_bsdf(
        pt::Path &path,
        auto &sampler,
        auto &hit,
        auto procedural_geometry,
        MediumTransition const &boundary,
        float u_lobe,
        bool sample_bsdf,
        bool need_albedo) {
        auto type = std::to_underlying(
            mtl::detect_polymorphic_bsdf_type(
                si.basic_param.weight));
        mtl::PolymorphicBSDF::visit(
            type,
            []<class Entry>(
                PathVertex &vertex,
                pt::Path &path,
                auto &sampler,
                auto &hit,
                auto procedural_geometry,
                MediumTransition const &boundary,
                float u_lobe,
                bool sample_bsdf,
                bool need_albedo) {
                using TypePair = typename Entry::type;
                using MatBSDF = typename TypePair::first_type;
                using MatExtraParameter =
                    typename TypePair::second_type;
                BSDFInstance<
                    MatBSDF,
                    MatExtraParameter,
                    transport_mode>
                    bsdf;
                vertex.template sample_bsdf<
                    transport_mode,
                    sample_lights>(
                    bsdf,
                    path,
                    sampler,
                    hit,
                    procedural_geometry,
                    boundary,
                    u_lobe,
                    sample_bsdf,
                    need_albedo);
            },
            *this,
            path,
            sampler,
            hit,
            procedural_geometry,
            boundary,
            u_lobe,
            sample_bsdf,
            need_albedo);
    }

    template<
        mtl::TransportMode transport_mode,
        bool sample_lights,
        class BSDF>
    void sample_bsdf(
        BSDF &bsdf,
        pt::Path &path,
        auto &sampler,
        auto &hit,
        auto procedural_geometry,
        MediumTransition const &boundary,
        float u_lobe,
        bool evaluate_bsdf,
        bool need_albedo) {
        bsdf.load(path, procedural_geometry, si);
        if (boundary.is_false) {
            enter_false_boundary(
                bsdf,
                path,
                boundary);
            _direct_radiance = 0.0f;
            _can_spawn = path.active;
            return;
        }

        si.emission = bsdf.eval_emission(si);
        float3 wi = -si.to_local(si.input_dir);
        auto context = bsdf.make_context(path, si);
        bsdf.configure_context(
            context,
            path,
            si,
            boundary);
        bsdf.initialize_context(
            context,
            si,
            float3(sampler.next2f(g_buffer_heap), u_lobe));

        if constexpr (transport_mode == mtl::TransportMode::Radiance) {
            if (si.is_indirect_ray && si.hit_triangle) {
                evaluate_light_hit(path);
            }
        }
        if (!evaluate_bsdf) return;

        bsdf.init(
            context,
            path,
            si,
            wi,
            need_albedo);
        if (!path.active) return;

        auto sampled = bsdf.sample(
            context,
            path.media,
            wi,
            float3(sampler.next2f(g_buffer_heap), u_lobe));
        if (!commit_sample(
                bsdf,
                context,
                path,
                boundary,
                sampled)) {
            return;
        }

        sample_direct_lighting<
            transport_mode,
            sample_lights>(
            bsdf,
            context,
            path,
            sampler,
            hit,
            wi);
        _can_spawn = true;
    }

    template<class BSDF>
    void enter_false_boundary(
        BSDF const &bsdf,
        pt::Path &path,
        MediumTransition const &boundary) {
        mtl::Volume medium;
        if (!bsdf.fill_medium(
                medium,
                boundary.uses_subsurface)) {
            path.active = false;
            path.beta = 0.0f;
            return;
        }
        bool selected_wavelength_now =
            bsdf.select_medium_wavelength(
                medium,
                path.spectrum,
                boundary.uses_subsurface,
                si.basic_param.specular.ior);
        medium.boundary_id = boundary.id;
        medium.nested_priority =
            si.basic_param.geometry.nested_priority;
        if (!mtl::active_medium_insert(path.media, medium)) {
            path.active = false;
            path.beta = 0.0f;
            return;
        }
        if (selected_wavelength_now) {
            mtl::collapse_active_medium_wavelengths(
                path.media,
                path.spectrum.hero_index);
        }
        si.sample_flags = mtl::BSDFFlags::NoMediumChange;
        _outgoing_direction = si.input_dir;
    }

    template<class BSDF>
    bool commit_sample(
        BSDF &bsdf,
        typename BSDF::Context const &context,
        pt::Path &path,
        MediumTransition const &boundary,
        mtl::BSDFSample const &sample) {
        bool selected_wavelength_now =
            path.update_wavelength(context);
        si.set_bsdf_sample(sample);
        if (!path.accept(sample)) return false;

        _outgoing_direction = si.to_world(sample.wo);
        _relocated = bsdf.relocate(si);
        if (!_relocated) {
            _outgoing_direction = si.bend_outgoing(
                boundary,
                _outgoing_direction);
            if (!si.basic_param.geometry.thin_walled &&
                mtl::is_transmissive(sample.throughput.flags)) {
                si.update_medium(
                    path,
                    bsdf,
                    boundary,
                    sample,
                    context);
                if (!path.active) return false;
            }
        }

        path.collapse_wavelengths(selected_wavelength_now);
        path.update_throughput(
            sample,
            _outgoing_bsdf_pdf);
        si.update_ray_offset(_relocated);
        path.update_detail(
            sample.throughput.flags,
            _relocated);
        return true;
    }

    template<
        mtl::TransportMode transport_mode,
        bool sample_lights,
        class BSDF>
    void sample_direct_lighting(
        BSDF &bsdf,
        typename BSDF::Context &context,
        pt::Path &path,
        auto &sampler,
        auto &hit,
        float3 wi) {
        if constexpr (
            transport_mode == mtl::TransportMode::Radiance &&
            sample_lights) {
            if (_outgoing_bsdf_pdf > 1e-4f) {
                DirectLighting lighting(
                    _relocated
                        ? si.plane_normal
                        : si.shading_normal(),
                    _outgoing_direction,
                    si.roughness,
                    !_relocated &&
                        mtl::is_specular(si.sample_flags),
                    !si.is_indirect_ray);
                auto light_sample = lighting.sample(
                    path.spectrum,
                    bsdf,
                    context,
                    wi,
                    sampler,
                    si,
                    hit);
                _direct_radiance = light_sample.radiance.xyz;
            } else {
                _direct_radiance = 0.0f;
            }
        } else {
            _direct_radiance = 0.0f;
        }
    }

    void evaluate_light_hit(pt::Path &path) {
        auto type = si.inst_info.get_light_mask();
        auto id = si.inst_info.get_light_id();
        if (type >= lighting::LightTypes::LightCount) return;

        lighting::PolymorphicLight::visit(
            type,
            []<class Entry>(
                PathVertex &vertex,
                pt::Path &path,
                uint id) {
                using Light = typename Entry::type;
                Light::eval_hit(
                    id,
                    vertex.si,
                    path,
                    vertex._incoming_bsdf_pdf);
            },
            *this,
            path,
            id);
    }

private:
    float3 _throughput;
    float3 _direct_radiance;
    float3 _outgoing_direction;
    float _incoming_bsdf_pdf;
    float _outgoing_bsdf_pdf;
    bool _can_spawn;
    bool _relocated = false;
    bool _selected_wavelength;
};

}// namespace integrator
