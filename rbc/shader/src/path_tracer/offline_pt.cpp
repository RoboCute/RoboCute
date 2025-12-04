#define DEBUG
#include <luisa/printer.hpp>

#define OFFLINE_MODE
#define RBC_USE_RAYQUERY
#define RBC_USE_RAYQUERY_SHADOW
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/read_pixel.hpp>
#include <sampling/sample_funcs.hpp>
#include <geometry/vertices.hpp>
#include <material/mats.hpp>
#include <utils/onb.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <lighting/importance_sampling.hpp>
#include <path_tracer/pt_args.hpp>

#include <path_tracer/integrator.hpp>
#include <path_tracer/gbuffer.hpp>

#include <std/inplace_vector>
#include <volumetric/volume.hpp>
#include <volumetric/trace.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &emission_img,
    Buffer<GBuffer> gbuffers,
#if defined(OFFLINE_DENOISER)
    Buffer<float> albedo_buffer,
    Buffer<float> normal_buffer,
#endif
    Buffer<float4x4> &last_transform_buffer,
    Buffer<MultiBouncePixel> &multi_bounce_pixel,
    Buffer<uint> &multi_bounce_pixel_counter,
    PTArgs args,
    uint2 size) {

    auto coord = dispatch_id().xy;
    // auto screen_uv = (float2(coord) + args.jitter_offset + float2(0.5)) / float2(size);
    float3 dir;
    Ray ray;
    /////////////// RR
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
    /////////////// RR
    sampling::HeitzSobol sampler(coord, args.frame_index);
    sampling::PCGSamplerOffsetted pcg_sampler(uint3(dispatch_id().xy, args.frame_index));
    pcg_sampler.offset = sampler.next3f(g_buffer_heap);
    auto screen_uv = (float2(coord) + sampling::sample_uniform_disk_concentric(pcg_sampler.next2f()) + 0.5f) / float2(size);
    {
        auto proj = float4((screen_uv * 2.f - 1.0f), 0.f, 1);
        auto world_pos = args.inv_vp * proj;
        world_pos /= world_pos.w;
        auto n_dir = world_pos.xyz - args.cam_pos;
        auto dir_len = length(n_dir);
        dir = n_dir / max(1e-4f, dir_len);
        proj.z = 1.0f;
        auto near_world_pos = args.inv_vp * proj;
        near_world_pos /= near_world_pos.w;
        if (args.enable_physical_camera) {
            auto coord_lens = sampling::sample_uniform_disk_concentric(pcg_sampler.next2f()) * args.lens_radius;
            auto p_lens = float3(coord_lens, 0.f);
            float3 dst_pos = near_world_pos.xyz + dir * args.focus_distance;
            float4 p_lens_r = args.inv_view * float4(p_lens, 1.0f);
            p_lens_r /= p_lens_r.w;
            near_world_pos = p_lens_r;
            dir = normalize(dst_pos - near_world_pos.xyz);
        }
        ray = Ray(near_world_pos.xyz, dir, sampling::offset_ray_t_min, dir_len);
    }
    ProceduralGeometry procedural_geometry;
    auto hit = rbc_trace_closest(ray, args, sampler, procedural_geometry);
    float3 addition_color;
    float addition_alpha = 0;
    float3 albedo_sum;
    float4 normal_rough;
    float4 hitpos;// xyz: pos, w: hit normal
    const int MAX_DEPTH = args.bounce + 1;

    float3 beta(1.f);
    float3 write_beta(-1.f);
    float first_dist;
    float3 radiance(0.f);
    auto write_tex = [&]() {
        if (any(coord >= size)) {
            return;
        }
        uint buffer_id = (coord.x + coord.y * size.x);
        GBuffer gbuffer;
        gbuffer.hitpos_normal = std::array<float, 4>(hitpos.x, hitpos.y, hitpos.z, hitpos.w);
        gbuffer.beta = std::array<float, 3>(write_beta.x, write_beta.y, write_beta.z);
        gbuffer.radiance = std::array<float, 3>(radiance.x, radiance.y, radiance.z);
        gbuffers.write(buffer_id, gbuffer);
#ifdef OFFLINE_DENOISER
        buffer_id *= 3;
        if (args.gbuffer_temporal_weight > 1e-8f) {
            float3 old_albedo;
            old_albedo.x = albedo_buffer.read(buffer_id);
            old_albedo.y = albedo_buffer.read(buffer_id + 1);
            old_albedo.z = albedo_buffer.read(buffer_id + 2);
            albedo_sum = lerp(albedo_sum, old_albedo, float3(args.gbuffer_temporal_weight));
            float3 old_normal;
            old_normal.x = normal_buffer.read(buffer_id);
            old_normal.y = normal_buffer.read(buffer_id + 1);
            old_normal.z = normal_buffer.read(buffer_id + 2);
            normal_rough.xyz = lerp(normal_rough.xyz, old_normal, float3(args.gbuffer_temporal_weight));
            auto norm_len = length(normal_rough.xyz);
            if (norm_len < 1e-5f) {
                normal_rough.xyz = old_normal;
            } else {
                normal_rough.xyz /= norm_len;
            }
        }
        albedo_buffer.write(buffer_id, albedo_sum.x);
        albedo_buffer.write(buffer_id + 1, albedo_sum.y);
        albedo_buffer.write(buffer_id + 2, albedo_sum.z);
        normal_buffer.write(buffer_id, normal_rough.x);
        normal_buffer.write(buffer_id + 1, normal_rough.y);
        normal_buffer.write(buffer_id + 2, normal_rough.z);
#endif

        addition_alpha = reject ? 0.f : 1.f;
        addition_color = reject ? float3(0.f) : addition_color;
        if (!args.reset_emission) {
            auto old_val = emission_img.read(coord);
            addition_color += old_val.xyz;
            addition_alpha += old_val.w;
        }
        emission_img.write(coord, float4(addition_color, addition_alpha));
    };

    std::inplace_vector<mtl::Volume, mtl::Volume::MAX_VOLUME_STACK_SIZE> volume_stack;
    SpectrumArg spectrum_arg;
    spectrum_arg.lambda = spectrum::sample_xyz(g_image_heap, fract(sampler.next(g_buffer_heap) + pcg_sampler.next() / 255.f));
    spectrum_arg.hero_index = pcg_sampler.nextui() % 3;
    if (hit.miss()) {

        normal_rough = float4(0, 0, 1, 0);
        if (args.sky_heap_idx != max_uint32) {
            addition_color = g_image_heap.uniform_idx_image_sample(args.sky_heap_idx, sampling::sphere_direction_to_uv(args.world_2_sky_mat, dir), Filter::LINEAR_POINT, Address::EDGE).xyz;
            addition_color = args.resource_to_rec2020_mat * addition_color;
            addition_color = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, addition_color);
        }
        write_tex();
        return 0;
    }

    beta = float3(1.0f);
    float3 input_pos = ray.origin();
    float pdf_bsdf = -1.0f;
    first_dist = 0.f;
    float2 ddx = float2(0);
    float2 ddy = float2(0);
    uint filter = Filter::ANISOTROPIC;
    float3 new_dir = dir;
    bool continue_loop = true;
    int depth = 0;
    float length_sum = 0.0f;
    int transparent_depth = 0;
    const int TRANS_MAX_DEPTH = 4;
    bool write_gbuffer = false;
    mtl::ShadingDetail detail = mtl::ShadingDetail::Default;
    vt::VTMeta vt_meta;
    vt_meta.frame_countdown = args.frame_countdown;
    float3 last_beta(0.f);
    float3 current_weight(0.f);
    // sampling::PCGSampler block_sampler(uint3(dispatch_id().xy / 8u, args.frame_index));
    while (depth < MAX_DEPTH) {
        float3 di_result;
        float di_dist;
        last_beta = beta;
        float lobe_rand;
        // if (write_gbuffer) {
        // 	lobe_rand = block_sampler.next();
        // } else
        {
            lobe_rand = sampler.next(g_buffer_heap);
        }
        bool selected_wavelength = spectrum_arg.selected_wavelength;
        IntegratorResult result = sample_material(
            pcg_sampler,
            volume_stack,
            length_sum,
            hit,
            procedural_geometry,
            spectrum_arg,
            args.world_2_sky_mat,
            vt_meta,
            lobe_rand,
            input_pos,
            beta,
            continue_loop,
            !write_gbuffer,
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
            reject);
        reject &= args.require_reject;
        continue_loop &= (!reject);
        if (length_sum >= 0.0f) {
            length_sum += hit.ray_t;
        }
        if (!selected_wavelength && spectrum_arg.selected_wavelength) {
            spectrum_arg.selected_wavelength = true;
            spectrum::modify_throughput(g_image_heap, spectrum_arg, beta, last_beta, di_result);
        }
        if (is_transmissive(result.sample_flags) && is_non_diffuse(result.sample_flags)) {
            if (transparent_depth < TRANS_MAX_DEPTH) {
                depth -= 1;
                ++transparent_depth;
            }
        }
        /////////// primary ray
        addition_color += result.emission * last_beta;
        current_weight = beta / max(float3(1e-4f), last_beta);
        if (!write_gbuffer) {
            ///////////// Record gbuffer in primary ray
            albedo_sum = result.albedo + spectrum::spectrum_to_tristimulus(result.emission);
            normal_rough = float4(result.normal, result.roughness);
        } else if (depth == args.bounce) {
            auto encoded_normal = sampling::encode_unit_vector(result.plane_normal);
            hitpos = float4(result.world_pos, bit_cast<float>((uint(encoded_normal.x * float(0xffff)) & 0xffff) + (uint(encoded_normal.y * float(0xffff)) << 16)));
            write_beta = beta;
            beta = float3(1);
            addition_color += radiance;
            radiance = float3(0.0f);
        }
        radiance += di_result * last_beta;
        write_gbuffer = true;
        if (reject) {
            radiance = 0.0f;
        }
        if (!continue_loop) {
            beta = float3(0.f);
            break;
        }

        ///////////// Prepare next ray
        ray = Ray(result.new_ray_offset + sampling::offset_ray_origin(result.world_pos, dot(result.plane_normal, new_dir) < 0 ? -result.plane_normal : result.plane_normal), new_dir, sampling::offset_ray_t_min);

        auto accum_sky = [&]() {
            if (args.sky_heap_idx != max_uint32) {
                float2 uv = sampling::sphere_direction_to_uv(args.world_2_sky_mat, new_dir);
                float theta;
                float phi;
                float3 wi;
                sampling::sphere_uv_to_direction_theta(args.world_2_sky_mat, uv, theta, phi, wi);
                float mis = 1.f;
                if (pdf_bsdf > 0.f) {
                    auto tex_size = g_image_heap.uniform_idx_image_size(args.sky_heap_idx);
                    auto sky_coord = int2(uv * float2(tex_size));
                    sky_coord = clamp(sky_coord, int2(0), int2(tex_size) - 1);
                    auto sky_pdf = g_buffer_heap.uniform_idx_buffer_read<float>(args.pdf_table_idx, sky_coord.y * tex_size.x + sky_coord.x);
                    sky_pdf = _directional_pdf(sky_pdf, theta);
                    mis = sampling::balanced_heuristic(pdf_bsdf, sky_pdf);
                }
                float3 sky_col = g_image_heap.uniform_idx_image_sample(args.sky_heap_idx, uv, Filter::LINEAR_POINT, Address::EDGE).xyz;
                sky_col = args.resource_to_rec2020_mat * sky_col;
                sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);
                radiance += beta * mis * sky_col;
            }
            if (depth < args.bounce) {
                hitpos = float4(new_dir, 0.f);
                addition_color += radiance;
                radiance = float3(0.0f);
                write_beta = float3(-1);
            }
        };

        if (depth < (MAX_DEPTH - 1)) {
            hit = mtl::trace_volumetric(volume_stack, beta, detail, ray, args, sampler, procedural_geometry, depth < 0 ? 255u : ONLY_OPAQUE_MASK);

            if (hit.miss()) {
                accum_sky();
                beta = float3(0);
                if (depth == 0) {
                    first_dist = 1e8f;
                }
                break;
            } else if (depth == 0) {
                first_dist = hit.ray_t;
            }
        } else {
            break;
        }
        input_pos = ray.origin();
        ++depth;
        ///////////// Done prepare next ray

        ///////////// End Loop
    }
    if (depth < 0 && transparent_depth < TRANS_MAX_DEPTH) {
        if (args.sky_heap_idx != max_uint32 && reduce_sum(beta) > 1e-5f) {
            float3 sky_col = g_image_heap.uniform_idx_image_sample(args.sky_heap_idx,
                                                                   sampling::sphere_direction_to_uv(args.world_2_sky_mat, new_dir), Filter::LINEAR_POINT, Address::EDGE)
                                 .xyz;
            sky_col = args.resource_to_rec2020_mat * sky_col;
            sky_col = spectrum::emission_to_spectrum(g_image_heap, g_volume_heap, spectrum_arg, sky_col);

            radiance += beta * sky_col;
        }
        write_tex();
        return 0;
    }
    radiance = clamp(radiance, float3(0.f), float3(16384.0f));
    //////////////////// RR
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
            float rand = pcg_sampler.next();
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
    if (rr_scale > 1e-5f && quad_beta[quad_group_id + quad_local_id] > 1e-8f) {
        MultiBouncePixel pixel;
        pixel.pixel_id = ((coord.x << 16u) | coord.y);
        beta *= quad_beta[quad_group_id + quad_local_id] / max(0.1f, rr_scale);
        pixel.beta[0] = beta.x;
        pixel.beta[1] = beta.y;
        pixel.beta[2] = beta.z;
        pixel.input_pos[0] = ray._origin[0];
        pixel.input_pos[1] = ray._origin[1];
        pixel.input_pos[2] = ray._origin[2];
        pixel.pdf_bsdf = pdf_bsdf;
        pixel.input_dir[0] = ray._dir[0];
        pixel.input_dir[1] = ray._dir[1];
        pixel.input_dir[2] = ray._dir[2];
        pixel.length_sum = length_sum;
        auto index = multi_bounce_pixel_counter.atomic_fetch_add(0, 1);
        multi_bounce_pixel.write(index, pixel);
    }
    //////////////////// RR
    write_tex();
    return 0;
}