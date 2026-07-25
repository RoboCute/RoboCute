#define OFFLINE_MODE
#include <path_tracer/integrator.hpp>
#include <path_tracer/pt_args.hpp>
#include <path_tracer/gbuffer.hpp>

#include <std/inplace_vector>
#include <volumetric/medium_probe.hpp>
#include <volumetric/trace.hpp>
#include <volumetric/volume.hpp>

void accum_sky(SpectrumArg &spectrum_arg,
               float3x3 world_2_sky_mat,
               uint sky_heap_idx,
               uint pdf_table_idx,
               float pdf_bsdf,
               float3 beta,
               float3 sample_dir,
               float3x3 resource_to_rec2020_mat,
               float3 &radiance) {
    if (sky_heap_idx == max_uint32) return;
    float2 uv = sampling::sphere_direction_to_uv(world_2_sky_mat, sample_dir);
    float theta;
    float phi;
    float3 wi;
    sampling::sphere_uv_to_direction_theta(world_2_sky_mat, uv, theta, phi, wi);
    float mis = 1.f;
    if (pdf_bsdf > 0.f) {
        auto tex_size = g_image_heap.uniform_idx_image_size(sky_heap_idx);
        auto sky_coord = int2(uv * float2(tex_size));
        sky_coord = clamp(sky_coord, int2(0), int2(tex_size) - 1);
        auto sky_pdf = g_buffer_heap.uniform_idx_buffer_read<float>(pdf_table_idx, sky_coord.y * tex_size.x + sky_coord.x);
        sky_pdf = _directional_pdf(sky_pdf, theta);
        mis = sampling::balanced_heuristic(pdf_bsdf, sky_pdf);
    }
    float3 sky_col = g_image_heap.uniform_idx_image_sample(sky_heap_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
    sky_col = resource_to_rec2020_mat * sky_col;
    sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);
    radiance += beta * mis * sky_col;
};
[[kernel_1d(128)]] int kernel(
    Buffer<MultiBouncePixel> &multi_bounce_pixel,
    Buffer<uint> &multi_bounce_pixel_counter,
    Buffer<GBuffer> &gbuffers,
    PTArgs args,
    uint2 size) {

    // return 0;
    auto id = dispatch_id().x;
    if (id >= multi_bounce_pixel_counter.read(0)) {
        return 0;
    }
    sampling::PCGSampler block_sampler(uint2(dispatch_id().x / 64u, args.frame_index));
    auto pixel = multi_bounce_pixel.read(id);
    uint2 coord(pixel.pixel_id >> 16u, pixel.pixel_id & 65535u);
    const uint MAX_DEPTH = args.bounce;
    uint depth = 0;
    float3 beta(pixel.beta[0], pixel.beta[1], pixel.beta[2]);
    sampling::PCGSampler pcg_sampler(uint2(id, args.frame_index));
    float3 input_pos(pixel.input_pos);
    bool continue_loop = true;
    uint filter = Filter::POINT;
    float2 ddx(1.0f);
    float2 ddy(1.0f);
    float pdf_bsdf = pixel.pdf_bsdf;
    float3 new_dir(pixel.input_dir);
    uint buffer_id = (coord.x + coord.y * size.x);
    auto gb = gbuffers.read(buffer_id);
    float3 radiance(gb.radiance[0], gb.radiance[1], gb.radiance[2]);
    Ray ray(input_pos, new_dir, sampling::offset_ray_t_min);
    vt::VTMeta vt_meta;
    vt_meta.frame_countdown = args.frame_countdown;

    mtl::ShadingDetail detail = mtl::ShadingDetail::IndirectDiffuse;
    std::inplace_vector<mtl::Volume, mtl::Volume::MAX_VOLUME_STACK_SIZE> volume_stack;
    SpectrumArg spectrum_arg;
    spectrum_arg.hero_index = unpack_hero_index(pixel.spectrum_state);
    spectrum_arg.selected_wavelength = unpack_selected_wavelength(pixel.spectrum_state);
    spectrum_arg.lambda = spectrum::sample_wavelengths(
        g_image_heap,
        unpack_wavelength_sample(pixel.spectrum_state));
    if (spectrum_arg.selected_wavelength) {
        spectrum_arg.lambda = spectrum_arg.lambda[spectrum_arg.hero_index];
    }
    float3 last_beta = 0;
    ProceduralGeometry procedural_geometry;
    bool reject = false;
    uint medium_boundary_steps = 0u;

    auto expected_volume_count = unpack_volume_count(pixel.spectrum_state);
    if (expected_volume_count > 0u) {
        bool selected_wavelength = spectrum_arg.selected_wavelength;
        bool probe_complete = mtl::probe_medium_stack(
            volume_stack,
            input_pos,
            args,
            pcg_sampler,
            spectrum_arg,
            args.resource_to_rec2020_mat,
            ONLY_OPAQUE_MASK);
        if (!probe_complete || volume_stack.size() != expected_volume_count) {
            return 0;
        }
        if (!selected_wavelength && spectrum_arg.selected_wavelength) {
            float3 ignored_last_beta = beta;
            float3 ignored_di = 0.0f;
            spectrum::modify_throughput(
                g_image_heap,
                spectrum_arg,
                args.spectrum,
                beta,
                ignored_last_beta,
                ignored_di);
        }
    }

    while (depth <= MAX_DEPTH) {
        auto trace_origin = ray.origin();
        auto hit = mtl::trace_volumetric(
            volume_stack,
            beta,
            detail,
            ray,
            args,
            pcg_sampler,
            procedural_geometry,
            ONLY_OPAQUE_MASK);
        bool volume_scattered = any(ray.origin() != trace_origin);
        if (volume_scattered) {
            input_pos = ray.origin();
            pdf_bsdf = -1.0f;
        }
        new_dir = ray.dir();
        if (hit.miss()) {
            accum_sky(
                spectrum_arg,
                args.world_2_sky_mat,
                args.sky_heap_idx,
                args.pdf_table_idx,
                pdf_bsdf,
                beta,
                new_dir,
                args.resource_to_rec2020_mat,
                radiance);
            break;
        }
        last_beta = beta;
        float3 di_result;
        float di_dist;
        uint mat_id;
        bool evaluate_bsdf = depth < MAX_DEPTH;
        bool selected_wavelength = spectrum_arg.selected_wavelength;
        IntegratorResult result = sample_material(
            pcg_sampler,
            volume_stack,
            hit,
            procedural_geometry,
            spectrum_arg,
            args.world_2_sky_mat,
            vt_meta,
            evaluate_bsdf ? block_sampler.next() : 0.0f,
            input_pos,
            beta,
            continue_loop,
            evaluate_bsdf,
            false,
            filter,
            ddx,
            ddy,
            detail,
            args,
            true,
            args.resource_to_rec2020_mat,
            di_result,
            di_dist,
            pdf_bsdf,
            new_dir,
            reject,
            mat_id);
        if (!selected_wavelength && spectrum_arg.selected_wavelength) {
            spectrum::modify_throughput(
                g_image_heap,
                spectrum_arg,
                args.spectrum,
                beta,
                last_beta,
                di_result);
        }
        reject &= args.require_reject;
        continue_loop &= (!reject);
        if (mtl::is_no_medium_change(result.sample_flags)) {
            if (++medium_boundary_steps >= mtl::MAX_MEDIUM_BOUNDARY_STEPS) break;
            if (!continue_loop) break;
            ray = Ray(
                sampling::offset_ray_origin(
                    result.world_pos,
                    dot(result.plane_normal, new_dir) < 0.0f ? -result.plane_normal : result.plane_normal),
                new_dir,
                sampling::offset_ray_t_min);
            continue;
        }
        medium_boundary_steps = 0u;
        radiance += result.emission * last_beta;
        if (!evaluate_bsdf) break;

        radiance += di_result * last_beta;
        // see integrate_hashgrid_offline.cpp
        if (reject) {
            radiance = float3(-1000.0f);
        }
        if (!continue_loop) {
            break;
        }
        ///////////// Prepare next ray
        ray = Ray(result.new_ray_offset + sampling::offset_ray_origin(result.world_pos, dot(result.plane_normal, new_dir) < 0 ? -result.plane_normal : result.plane_normal), new_dir, 0.0f);
        input_pos = ray.origin();
        ++depth;
    }
    gb.radiance = std::array<float, 3>(radiance.x, radiance.y, radiance.z);
    gbuffers.write(buffer_id, gb);
    return 0;
}
