#pragma once

#include <luisa/std.hpp>
#include <path_tracer/gbuffer.hpp>
#include <path_tracer/path.hpp>
#include <path_tracer/path_vertex.hpp>
#include <path_tracer/pt_args.hpp>
#include <spectrum/spectrum.hpp>

using namespace luisa::shader;

namespace pt {

class PrimaryOutput {
public:
    PrimaryOutput() {
        primary_hit_written = false;
        addition_color = float3(0.0f);
        emission_sum = float3(0.0f);
        gbuffer_albedo = float3(0.0f);
        gbuffer_uv = float2(0.0f);
#ifdef RBC_OFFLINE_PT_DENOISE
        albedo_sum = float3(0.0f);
#endif
        normal_rough = float4(0.0f);
        to_cam_dist = 1e28f;
        obj_id = uint2(max_uint32, max_uint32);
        write_beta = float3(-1.0f);
        radiance = float3(0.0f);
    }

    bool primary_hit_written;
    float3 addition_color;
    float3 emission_sum;
    float3 gbuffer_albedo;
    float2 gbuffer_uv;
#ifdef RBC_OFFLINE_PT_DENOISE
    float3 albedo_sum;
#endif
    float4 normal_rough;
    float to_cam_dist;
    uint2 obj_id;
    uint mat_id_channel;
    float2 obj_bary;
    float4 hitpos;// xyz: position, w: encoded hit normal
    float3 write_beta;
    float3 radiance;

    void write_primary_hit(uint2 coord, CommittedHit const &primary) {
        if (!args.write_id_map || primary_hit_written) return;
        uint4 encoded_hit(max_uint32, max_uint32, 0, 0);
        if (primary.hit_triangle()) {
            encoded_hit.x = primary.inst;
            encoded_hit.y = primary.prim;
            encoded_hit.z = bit_cast<uint>(primary.bary.x);
            encoded_hit.w = bit_cast<uint>(primary.bary.y);
        } else if (primary.hit_procedural()) {
            encoded_hit.x = primary.inst;
            encoded_hit.y = primary.prim;
        }
        id_map.write(coord, encoded_hit);
        primary_hit_written = true;
    }

    void record_surface(
        Path &path,
        uint2 coord,
        CommittedHit const &hit,
        integrator::PathVertex const &vertex,
        bool &write_gbuffer,
        int &depth,
        int &transparent_depth,
        int transparent_depth_limit,
        bool reject,
        float3 &current_weight) {
        write_primary_hit(coord, hit);
        if (is_transmissive(vertex.si.sample_flags) &&
            is_non_diffuse(vertex.si.sample_flags) &&
            transparent_depth < transparent_depth_limit) {
            depth -= 1;
            ++transparent_depth;
        }

        addition_color +=
            vertex.si.emission * vertex.throughput();
        current_weight =
            path.beta / max(float3(1e-4f), vertex.throughput());
        // Keep refractive radiance compression out of continuation probability.
        current_weight *= sqr(vertex.si.eta);
        if (!write_gbuffer) {
            gbuffer_albedo = vertex.si.albedo;
            gbuffer_uv = vertex.si.uv[0];
#ifdef RBC_OFFLINE_PT_DENOISE
            albedo_sum =
                vertex.si.albedo +
                spectrum::spectrum_to_tristimulus(
                    vertex.si.emission,
                    args.spectrum);
#endif
            emission_sum = vertex.si.emission;
            normal_rough = float4(
                vertex.si.shading_normal(),
                vertex.si.roughness);
            to_cam_dist = hit.ray_t;
            obj_id.x = vertex.si.user_id;
            obj_id.y = hit.prim;
            obj_bary = hit.bary;
            mat_id_channel = vertex.si.material_id;
        } else if (depth == args.bounce) {
            auto encoded_normal = sampling::encode_unit_vector(
                vertex.si.plane_normal);
            hitpos = float4(
                vertex.si.world_pos,
                bit_cast<float>(
                    (uint(encoded_normal.x * float(0xffff)) & 0xffff) +
                    (uint(encoded_normal.y * float(0xffff)) << 16)));
            write_beta = path.beta;
            path.beta = float3(1.0f);
            addition_color += radiance;
            radiance = float3(0.0f);
        }
        radiance +=
            vertex.direct_radiance() * vertex.throughput();
        write_gbuffer = true;
        if (reject) {
            radiance = 0.0f;
        }
    }

    void write(uint2 coord, bool reject) {
        if (any(coord >= size)) {
            return;
        }
        uint buffer_id = coord.x + coord.y * size.x;
        GBuffer gbuffer;
        gbuffer.hitpos_normal = std::array<float, 4>(
            hitpos.x,
            hitpos.y,
            hitpos.z,
            hitpos.w);
        gbuffer.beta = std::array<float, 3>(
            write_beta.x,
            write_beta.y,
            write_beta.z);
        gbuffer.radiance = std::array<float, 3>(
            radiance.x,
            radiance.y,
            radiance.z);
        gbuffers.write(buffer_id, gbuffer);
        float alpha = reject ? 0.0f : 1.0f;
        addition_color = reject ? float3(0.0f) : addition_color;
        if (!args.reset_emission) {
            auto old_val = emission_img.read(coord);
            addition_color += old_val.xyz;
            alpha += old_val.w;
        }
        float emission_alpha = alpha;
        float mask = 0.0f;
        switch (alpha_option) {
            case 1:
                mask = to_cam_dist > 1e10f ? 0.0f : 1.0f;
                break;
            case 2:
                mask = to_cam_dist > 1e20f ? 1.0f : 0.0f;
                break;
            case 3:
                mask = 0.0f;
                break;
        }
        if (alpha_option > 0) {
            mask_img.write(coord, float4(mask));
        }
        emission_img.write(
            coord,
            float4(addition_color, emission_alpha));
        if (reject) return;

        alpha = args.frame_index == 0 ? 1.0f : (alpha / (last_img.read(coord).w + alpha));
#ifdef RBC_OFFLINE_PT_DENOISE
        {
            auto origin_buffer_id = buffer_id;
            buffer_id *= 3;
            if (alpha < 0.999f) {
                float3 old_albedo;
                old_albedo.x = albedo_buffer.read(buffer_id);
                old_albedo.y = albedo_buffer.read(buffer_id + 1);
                old_albedo.z = albedo_buffer.read(buffer_id + 2);
                albedo_sum = lerp(
                    old_albedo,
                    albedo_sum,
                    float3(alpha));
                float3 old_normal;
                old_normal.x = normal_buffer.read(buffer_id);
                old_normal.y = normal_buffer.read(buffer_id + 1);
                old_normal.z = normal_buffer.read(buffer_id + 2);
                normal_rough.xyz = lerp(
                    old_normal,
                    normal_rough.xyz,
                    float3(alpha));
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
            buffer_id = origin_buffer_id;
        }
#endif

        write_geometry_aovs(buffer_id);
    }

private:
    void write_geometry_aovs(uint buffer_id) const {
        uint byte_offset = 0;
        const uint pixel_count = size.x * size.y;
        if ((args.geometry_mask & (1 << 0)) != 0)// Depth
        {
            const uint element_size = 1;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, to_cam_dist);
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 1)) != 0)// Normal
        {
            const uint element_size = 3;
            float3 value = normal_rough.xyz * 0.5f + 0.5f;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, value.x);
            geometry_buffer.write(read_index + 1, value.y);
            geometry_buffer.write(read_index + 2, value.z);
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 2)) != 0)// Object ID
        {
            const uint element_size = 1;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(
                read_index,
                bit_cast<float>(obj_id.x));
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 3)) != 0)// Primitive ID
        {
            const uint element_size = 1;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(
                read_index,
                bit_cast<float>(obj_id.y));
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 4)) != 0)// Barycentric
        {
            const uint element_size = 2;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, obj_bary.x);
            geometry_buffer.write(read_index + 1, obj_bary.y);
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 5)) != 0)// Emission
        {
            const uint element_size = 3;
            float3 value = spectrum::spectrum_to_tristimulus(
                emission_sum,
                args.spectrum);
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, value.x);
            geometry_buffer.write(read_index + 1, value.y);
            geometry_buffer.write(read_index + 2, value.z);
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 6)) != 0)// Albedo
        {
            const uint element_size = 3;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, gbuffer_albedo.x);
            geometry_buffer.write(read_index + 1, gbuffer_albedo.y);
            geometry_buffer.write(read_index + 2, gbuffer_albedo.z);
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 7)) != 0)// Material ID
        {
            const uint element_size = 1;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(
                read_index,
                bit_cast<float>(mat_id_channel));
            byte_offset += element_size * pixel_count;
        }
        if ((args.geometry_mask & (1 << 8)) != 0)// UV
        {
            const uint element_size = 2;
            uint read_index = byte_offset + buffer_id * element_size;
            geometry_buffer.write(read_index, gbuffer_uv.x);
            geometry_buffer.write(read_index + 1, gbuffer_uv.y);
        }
    }
};

class ContinuationQueue {
public:
    ContinuationQueue(uint2 coord, float wavelength_sample)
        : _coord(coord),
          _wavelength_sample(wavelength_sample) {}

    void enqueue(
        Path &path,
        float selected_quad_weight,
        float rr_scale) const {
        if (!(rr_scale > 1e-5f && selected_quad_weight > 1e-8f)) return;

        MultiBouncePixel pixel;
        pixel.pixel_id = (_coord.x << 16u) | _coord.y;
        path.beta *= selected_quad_weight / max(0.1f, rr_scale);
        pixel.beta[0] = path.beta.x;
        pixel.beta[1] = path.beta.y;
        pixel.beta[2] = path.beta.z;
        pixel.input_pos[0] = path.ray._origin[0];
        pixel.input_pos[1] = path.ray._origin[1];
        pixel.input_pos[2] = path.ray._origin[2];
        pixel.pdf_bsdf = path.prev_bsdf_pdf;
        pixel.input_dir[0] = path.ray._dir[0];
        pixel.input_dir[1] = path.ray._dir[1];
        pixel.input_dir[2] = path.ray._dir[2];
        pixel.spectrum_state = pack_spectrum_state(
            _wavelength_sample,
            path.spectrum.hero_index,
            path.spectrum.selected_wavelength,
            path.media.size());
        auto index = multi_bounce_pixel_counter.atomic_fetch_add(0, 1);
        multi_bounce_pixel.write(index, pixel);
    }

private:
    uint2 _coord;
    float _wavelength_sample;
};

}// namespace pt
