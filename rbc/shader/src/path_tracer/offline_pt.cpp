#define OFFLINE_MODE
#include <luisa/resources.hpp>
#include <path_tracer/gbuffer.hpp>
#include <path_tracer/pt_args.hpp>

namespace luisa::shader {

extern PTArgs args;
extern uint2 size;
extern Buffer<MultiBouncePixel> &multi_bounce_pixel;
extern Buffer<uint> &multi_bounce_pixel_counter;
extern Buffer<GBuffer> &gbuffers;
extern Image<float> &emission_img;
extern Image<float> &last_img;
extern Image<uint> &id_map;
extern Image<float> &mask_img;
#ifdef RBC_OFFLINE_PT_DENOISE
extern Buffer<float> &albedo_buffer;
extern Buffer<float> &normal_buffer;
#endif
extern Buffer<float> &geometry_buffer;
extern int alpha_option;

}// namespace luisa::shader

#include <path_tracer/environment_light.hpp>
#include <path_tracer/path.hpp>
#include <path_tracer/path_vertex.hpp>
#include <path_tracer/primary_output.hpp>
#include <path_tracer/sample_streams.hpp>
#include <sampling/sample_funcs.hpp>
#include <volumetric/medium_probe.hpp>
#include <volumetric/trace.hpp>

using namespace luisa::shader;

namespace pt {

class PathIntegrator {
public:
    explicit PathIntegrator(uint2 coord) : _coord(coord) {}

    int sample() {

        auto coord = _coord;
        SharedArray<float, 16 * 8> quad_beta;
        SharedArray<uint, 8 * 4> quad_flags;
        bool reject = false;
        auto quad_id = thread_id().xy;
        quad_id /= 2u;
        auto quad_group_id = (quad_id.y * 8 + quad_id.x) * 4u;
        auto quad_local_id = (thread_id().x & 1u) + (thread_id().y & 1u) * 2;
        quad_beta[quad_group_id + quad_local_id] = 0;
        if (quad_local_id == 0) {
            quad_flags[quad_group_id / 4] = 0;
        }
        if (any(coord >= size)) {
            return 0;
        }
        pt::PrimarySampleStreams samplers(coord, args.frame_index);
        samplers.surface.offset = samplers.trace.next3f(g_buffer_heap) / 128.f;
        float3 dir;
        Ray ray = sample_camera_ray(samplers, dir);

        pt::Path path;
        float wavelength_sample = initialize_path(path, ray, samplers);
        probe_initial_medium(path, samplers);

        ProceduralGeometry procedural_geometry;
        CommittedHit hit = path.trace(
            samplers.trace,
            procedural_geometry);
        dir = path.ray.dir();
        pt::PrimaryOutput output;
        const int MAX_DEPTH = args.bounce + 1;

        float first_dist;
        pt::EnvironmentLight environment;

        if (hit.miss()) {
            output.write_primary_hit(coord, hit);
            output.normal_rough = float4(0, 0, 1, 0);
            if (args.sky_heap_idx != max_uint32) {
                output.addition_color = environment.eval(
                    path.spectrum,
                    dir);
                output.addition_color *= path.beta;
            }
            output.emission_sum = output.addition_color;
            output.write(coord, reject);
            return 0;
        }

        first_dist = 0.f;
        float3 new_dir = dir;
        int depth = 0;
        int transparent_depth = 0;
        const int TRANS_MAX_DEPTH = 4;
        bool write_gbuffer = false;
        float3 last_beta(0.f);
        float3 current_weight(0.f);
        while (depth < MAX_DEPTH) {
            float lobe_rand = samplers.next_lobe();
            integrator::PathVertex vertex;
            vertex.sample<
                mtl::TransportMode::Radiance,
                true>(
                path,
                samplers.surface,
                hit,
                procedural_geometry,
                lobe_rand,
                true,
                !write_gbuffer,
                Filter::ANISOTROPIC,
                reject);
            vertex.apply_rejection(path);
            vertex.apply_wavelength_transition(path);
            bool no_medium_change = vertex.no_medium_change();
            bool boundary_limit =
                path.update_medium_boundary(no_medium_change);
            if (boundary_limit) {
                path.active = false;
                path.beta = 0.0f;
            }

            last_beta = vertex.throughput();
            new_dir = vertex.outgoing_direction();
            reject = vertex.si.reject;
            if (!no_medium_change) {
                output.record_surface(
                    path,
                    coord,
                    hit,
                    vertex,
                    write_gbuffer,
                    depth,
                    transparent_depth,
                    TRANS_MAX_DEPTH,
                    reject,
                    current_weight);
            }
            if (boundary_limit) {
                break;
            }
            if (!path.active) {
                path.beta = float3(0.0f);
                break;
            }
            if (!vertex.advance(
                    path,
                    sampling::offset_ray_t_min,
                    depth < (MAX_DEPTH - 1))) break;
            hit = path.trace(
                samplers.trace,
                procedural_geometry,
                depth < 0 ? 255u : ONLY_OPAQUE_MASK);
            if (hit.miss()) {
                output.write_primary_hit(coord, hit);
                output.radiance +=
                    environment.eval_escaped_ray(
                        path.spectrum,
                        path.prev_bsdf_pdf,
                        path.beta,
                        vertex.outgoing_direction());
                path.beta = float3(0.0f);
                if (depth < args.bounce) {
                    output.hitpos = float4(new_dir, 0.f);
                    output.addition_color += output.radiance;
                    output.radiance = float3(0.0f);
                    output.write_beta = float3(-1);
                }
                if (depth == 0) {
                    first_dist = 1e8f;
                }
                break;
            } else if (depth == 0) {
                first_dist = hit.ray_t;
            }
            if (!no_medium_change) ++depth;
        }
        if (!output.primary_hit_written && args.write_id_map) {
            id_map.write(coord, uint4(max_uint32, max_uint32, 0, 0));
        }
        if (depth < 0 && transparent_depth < TRANS_MAX_DEPTH) {
            if (args.sky_heap_idx != max_uint32 &&
                reduce_sum(path.beta) > 1e-5f) {
                output.radiance +=
                    path.beta * environment.eval(
                                    path.spectrum,
                                    new_dir);
            }
            output.write(coord, reject);
            return 0;
        }
        output.radiance = clamp(
            output.radiance,
            float3(0.f),
            float3(16384.0f));
        sync_block();
        // from Physically Based Shader Design in Arnold, Langlands, 2014
        float rr_scale = reduce_max(last_beta * current_weight) / max(1e-4f, reduce_max(last_beta));
        rr_scale = min(1.f, sqrt(rr_scale));
        quad_beta[quad_group_id + quad_local_id] = rr_scale;
        sync_block();
        if (quad_flags.atomic_fetch_add(quad_group_id / 4, 1) == 0) {
            float sum = 0.f;
            for (int i = 0; i < 4; ++i) {
                sum += quad_beta[quad_group_id + i];
            }
            if (sum > 1e-5f) {
                float rand = samplers.surface.next();
                int i = 0;
                while (i < 4) {
                    float num = quad_beta[quad_group_id + i] / sum;
                    if (rand <= num) {
                        quad_beta[quad_group_id + i] = sum;
                        ++i;
                        break;
                    } else {
                        rand -= num;
                        quad_beta[quad_group_id + i] = -1.0f;
                    }
                    ++i;
                }
                while (i < 4) {
                    quad_beta[quad_group_id + i] = -1.0f;
                    ++i;
                }
            } else {
                for (int i = 0; i < 4; ++i) {
                    quad_beta[quad_group_id + i] = -1.0f;
                }
            }
        }
        sync_block();
        pt::ContinuationQueue continuation_queue(coord, wavelength_sample);
        continuation_queue.enqueue(
            path,
            quad_beta[quad_group_id + quad_local_id],
            rr_scale);
        output.write(coord, reject);
        return 0;
    }

private:
    Ray sample_camera_ray(
        PrimarySampleStreams &samplers,
        float3 &direction) const {
        auto screen_uv =
            (float2(_coord) +
             sampling::sample_uniform_disk_concentric(
                 samplers.surface.next2f()) +
             0.5f) /
            float2(size);
        auto projection = float4((screen_uv * 2.0f - 1.0f), 0.0f, 1.0f);
        auto world_position = args.inv_vp * projection;
        world_position /= world_position.w;
        auto unnormalized_direction = world_position.xyz - args.cam_pos;
        auto direction_length = length(unnormalized_direction);
        direction = unnormalized_direction / max(1e-4f, direction_length);

        projection.z = 1.0f;
        auto near_world_position = args.inv_vp * projection;
        near_world_position /= near_world_position.w;
        if (args.enable_physical_camera) {
            auto lens_coord = sampling::sample_uniform_disk_concentric(
                                  samplers.surface.next2f()) *
                              args.lens_radius;
            auto lens_position = float3(lens_coord, 0.0f);
            float3 focus_position =
                near_world_position.xyz + direction * args.focus_distance;
            float4 world_lens_position =
                args.inv_view * float4(lens_position, 1.0f);
            world_lens_position /= world_lens_position.w;
            near_world_position = world_lens_position;
            direction = normalize(focus_position - near_world_position.xyz);
        }
        return Ray(
            near_world_position.xyz,
            direction,
            sampling::offset_ray_t_min,
            direction_length);
    }

    float initialize_path(
        Path &path,
        Ray ray,
        PrimarySampleStreams &samplers) const {
        path.ray = ray;
        path.segment_origin = ray.origin();
        path.beta = float3(1.0f);
        path.prev_bsdf_pdf = -1.0f;
        path.detail = mtl::ShadingDetail::Default;
        path.medium_boundary_steps = 0u;
        path.active = true;
        float wavelength_sample = fract(
            samplers.trace.next(g_buffer_heap) +
            samplers.surface.next() / 255.0f);
        path.spectrum.lambda = spectrum::sample_wavelengths(
            g_image_heap,
            wavelength_sample);
        path.spectrum.hero_index = samplers.surface.nextui() % 3u;
        return wavelength_sample;
    }

    void probe_initial_medium(
        Path &path,
        PrimarySampleStreams &samplers) const {
        if (!args.probe_initial_medium) return;

        bool selected_wavelength = path.spectrum.selected_wavelength;
        mtl::probe_medium_stack(
            path.media,
            path.ray.origin(),
            args,
            samplers.surface,
            path.spectrum,
            args.resource_to_rec2020_mat);
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
    }

    uint2 _coord;
};

}// namespace pt

[[kernel_2d(16, 8)]] int kernel() {
    pt::PathIntegrator integrator(dispatch_id().xy);
    return integrator.sample();
}
