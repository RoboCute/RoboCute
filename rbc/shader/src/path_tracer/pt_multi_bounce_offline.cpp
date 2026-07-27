#define OFFLINE_MODE
#include <path_tracer/gbuffer.hpp>
#include <path_tracer/pt_args.hpp>

namespace luisa::shader {

extern PTArgs args;
extern uint2 size;
extern Buffer<MultiBouncePixel> &multi_bounce_pixel;
extern Buffer<uint> &multi_bounce_pixel_counter;
extern Buffer<GBuffer> &gbuffers;

}// namespace luisa::shader

#include <path_tracer/environment_light.hpp>
#include <path_tracer/path.hpp>
#include <path_tracer/path_vertex.hpp>
#include <path_tracer/sample_streams.hpp>
#include <sampling/sample_funcs.hpp>
#include <volumetric/medium_probe.hpp>
#include <volumetric/trace.hpp>

namespace pt {

class ContinuationPathIntegrator {
public:
    explicit ContinuationPathIntegrator(uint id) : _id(id) {}

    int sample() {

        auto id = _id;
        if (id >= multi_bounce_pixel_counter.read(0)) {
            return 0;
        }
        auto pixel = multi_bounce_pixel.read(id);
        uint2 coord(pixel.pixel_id >> 16u, pixel.pixel_id & 65535u);
        const uint MAX_DEPTH = args.bounce;
        uint depth = 0;
        pt::ContinuationSampleStreams samplers(
            id,
            dispatch_id().x / 64u,
            args.frame_index);
        pt::Path path;
        path.beta = float3(pixel.beta[0], pixel.beta[1], pixel.beta[2]);
        path.segment_origin = float3(pixel.input_pos);
        path.active = true;
        path.prev_bsdf_pdf = pixel.pdf_bsdf;
        float3 new_dir(pixel.input_dir);
        uint buffer_id = (coord.x + coord.y * size.x);
        auto gb = gbuffers.read(buffer_id);
        float3 radiance(gb.radiance[0], gb.radiance[1], gb.radiance[2]);
        path.ray = Ray(path.segment_origin, new_dir, sampling::offset_ray_t_min);
        path.detail = mtl::ShadingDetail::IndirectDiffuse;
        path.spectrum.hero_index = unpack_hero_index(pixel.spectrum_state);
        path.spectrum.selected_wavelength = unpack_selected_wavelength(pixel.spectrum_state);
        path.spectrum.lambda = spectrum::sample_wavelengths(
            g_image_heap,
            unpack_wavelength_sample(pixel.spectrum_state));
        if (path.spectrum.selected_wavelength) {
            path.spectrum.lambda = path.spectrum.lambda[path.spectrum.hero_index];
        }
        ProceduralGeometry procedural_geometry;
        bool reject = false;
        path.medium_boundary_steps = 0u;
        pt::EnvironmentLight environment;

        if (!restore_initial_media(path, samplers, pixel)) return 0;

        while (depth <= MAX_DEPTH) {
            auto hit = path.trace(
                samplers.path,
                procedural_geometry,
                ONLY_OPAQUE_MASK);
            new_dir = path.ray.dir();
            if (hit.miss()) {
                environment.accumulate_escaped_ray(
                    path.spectrum,
                    path.prev_bsdf_pdf,
                    path.beta,
                    new_dir,
                    radiance);
                break;
            }

            bool sample_bsdf = depth < MAX_DEPTH;
            auto lobe_rand = samplers.next_lobe(sample_bsdf);
            integrator::PathVertex vertex;
            vertex.sample<
                mtl::TransportMode::Radiance,
                true>(
                path,
                samplers.path,
                hit,
                procedural_geometry,
                lobe_rand,
                sample_bsdf,
                false,
                Filter::POINT,
                reject);
            vertex.apply_wavelength_transition(path);
            vertex.apply_rejection(path);
            reject = vertex.si.reject;
            new_dir = vertex.outgoing_direction();

            bool no_medium_change = vertex.no_medium_change();
            if (no_medium_change) {
                if (path.update_medium_boundary(true)) {
                    break;
                }
                if (!path.active) break;
                if (!vertex.advance(path, 0.0f, true)) break;
                continue;
            }

            path.update_medium_boundary(false);
            radiance +=
                vertex.si.emission * vertex.throughput();
            if (!sample_bsdf) break;

            radiance +=
                vertex.direct_radiance() * vertex.throughput();
            // Preserve the rejection sentinel consumed by hash-grid integration.
            if (reject) {
                radiance = float3(-1000.0f);
            }
            if (!path.active) break;

            if (!vertex.advance(path, 0.0f, true)) break;
            ++depth;
        }
        gb.radiance = std::array<float, 3>(radiance.x, radiance.y, radiance.z);
        gbuffers.write(buffer_id, gb);
        return 0;
    }

private:
    bool restore_initial_media(
        Path &path,
        ContinuationSampleStreams &samplers,
        MultiBouncePixel const &pixel) const {
        auto expected_volume_count =
            unpack_volume_count(pixel.spectrum_state);
        if (expected_volume_count == 0u) return true;

        bool selected_wavelength = path.spectrum.selected_wavelength;
        bool probe_complete = mtl::probe_medium_stack(
            path.media,
            path.segment_origin,
            args,
            samplers.path,
            path.spectrum,
            args.resource_to_rec2020_mat,
            ONLY_OPAQUE_MASK);
        if (!probe_complete || path.media.size() != expected_volume_count) {
            return false;
        }
        if (!selected_wavelength && path.spectrum.selected_wavelength) {
            float3 ignored_last_beta = path.beta;
            float3 ignored_direct_lighting = 0.0f;
            spectrum::modify_throughput(
                g_image_heap,
                path.spectrum,
                args.spectrum,
                path.beta,
                ignored_last_beta,
                ignored_direct_lighting);
        }
        return true;
    }

    uint _id;
};

}// namespace pt

[[kernel_1d(128)]] int kernel() {
    pt::ContinuationPathIntegrator integrator(dispatch_id().x);
    return integrator.sample();
}
