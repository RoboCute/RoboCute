#pragma once
#include <luisa/std.hpp>
#include <material/mat_codes.hpp>
#include <material/openpbr_params.hpp>
#include <virtual_tex/stream.hpp>
#include "openpbr.hpp"

namespace material {
inline bool OpenPBR::cutout(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    uint mat_type,
    uint mat_index,
    vt::VTMeta vt_meta,
    float2 uv,
    int &priority,
    auto &rng) {
    auto geo = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Geometry>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, geometry));
    float geo_opacity = geo.opacity;
    auto read_tex = [&](auto uv, MatImageHandle const &tex) {
        uint min_level;
        return vt::sample_vt(
            buffer_heap,
            image_heap,
            vt_meta,
            tex,
            uv,
            0.0f,
            min_level,
            Filter::POINT, Address::REPEAT);
    };
    if (geo.opacity_tex.valid()) {
        auto uvs = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::UVs>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, uvs));
        uv = uv * float2(uvs.uv_scale) + float2(uvs.uv_offset);
        geo_opacity *= read_tex(uv, geo.opacity_tex).x;
    } else {
        auto base = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Base>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, base));
        if (base.albedo_tex.valid()) {
            auto uvs = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::UVs>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, uvs));
            uv = uv * float2(uvs.uv_scale) + float2(uvs.uv_offset);
            geo_opacity *= read_tex(uv, base.albedo_tex).w;
        }
    }
    if ((float)geo.cutout_threshold == 0.0f) {
        return geo_opacity < rng.next(buffer_heap);
    } else {
        return geo_opacity < (float)geo.cutout_threshold;
    }
}
inline float3 OpenPBR::get_emission(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    uint mat_type,
    uint mat_index,
    float2 uv) {
    auto emission = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Emission>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, emission));
    float3 col = float3(emission.luminance);
    if (emission.emission_tex.valid()) {
        auto uvs = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::UVs>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, uvs));
        uv = uv * float2(uvs.uv_scale) + float2(uvs.uv_offset);
        col *= image_heap.image_sample_level(emission.emission_tex, uv, 16, Filter::LINEAR_POINT, Address::REPEAT).xyz;
    }
    auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Weight>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, weight));
    if ((float)weight.coat > 0.f) {
        auto coat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Coat>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, coat));
        col *= lerp(float3(1), float3(coat.coat_color), float3(weight.coat));
    }
    return col;
};
inline bool OpenPBR::transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    uint mat_type,
    uint mat_index,
    auto &params,
    uint texture_filter,
    vt::VTMeta vt_meta,
    float2 uv,
    float4 ddxy,
    float3 input_dir,
    bool &reject,
    auto &&...) {
    auto uvs = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::UVs>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, uvs));
    uv = uv * float2(uvs.uv_scale) + float2(uvs.uv_offset);
    float2 ddx = ddxy.xy * float2(uvs.uv_scale);
    float2 ddy = ddxy.zw * float2(uvs.uv_scale);

    if (reject) return false;
    auto read_tex = [&](MatImageHandle const &tex) {
        uint min_level;
        uint dst_level;
        auto v = vt::sample_vt(
            buffer_heap,
            image_heap,
            vt_meta,
            tex,
            uv,
            ddx,
            ddy,
            min_level,
            dst_level,
            texture_filter, Address::REPEAT);
        reject |= min_level > dst_level;
        return v;
    };
    auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Weight>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, weight));
    // Weight
    if constexpr (requires { params.weight; }) {
        params.weight.base = weight.base;
        params.weight.diffuse_roughness = (float)weight.diffuse_roughness;
        params.weight.specular = (float)weight.specular;
        params.weight.metalness = weight.metallic;
        if (weight.metallic_roughness_tex.valid()) {
            auto tex_val = read_tex(weight.metallic_roughness_tex);

            params.weight.metalness *= tex_val.x;
            if constexpr (requires { params.specular; })
                params.specular.roughness *= tex_val.y;
        }
        params.weight.subsurface = weight.subsurface;
        params.weight.transmission = weight.transmission;
        params.weight.coat = weight.coat;
        params.weight.fuzz = weight.fuzz;
        params.weight.thin_film = weight.thin_film;
        params.weight.diffraction = weight.diffraction;
    }

    // Geometry
    if constexpr (requires { params.geometry; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Geometry>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, geometry));
        params.geometry.thin_walled = mat.thin_walled;
        params.geometry.thickness = (float)mat.thickness * 1e-2f;
        if (mat.normal_tex.valid()) {
            auto tan_normal = read_tex(mat.normal_tex).xyz;

            tan_normal.xy = (tan_normal.xy * 2.f - 1.f) * (float)mat.bump_scale;
            tan_normal = normalize(tan_normal);
            params.geometry.onb.replace_normal(tan_normal, -input_dir);
        }
    }

    // Specular
    if constexpr (requires { params.specular; })
    // if (params.specular.weight > 0.f)
    {

        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Specular>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, specular));
        params.specular.color = float3(mat.specular_color);
        params.specular.roughness *= (float)mat.roughness;
        params.specular.roughness_anisotropy = (float)mat.roughness_anisotropy;
        if (mat.specular_anisotropy_level_tex.valid()) {
            params.specular.roughness_anisotropy *= read_tex(mat.specular_anisotropy_level_tex).x;
        }
        params.geometry.onb.rotate_tangent((float)mat.roughness_anisotropy_angle);
        if (mat.specular_anisotropy_angle_tex.valid()) {
            auto tex_val = read_tex(mat.specular_anisotropy_angle_tex).xy;

            auto ls = length_squared(tex_val);
            flatten();
            if (ls == 0.0f) {
                params.specular.roughness_anisotropy = 0.0f;
            } else {
                params.geometry.onb.rotate_tangent(tex_val.xy * rsqrt(ls));
            }
        }
        params.specular.ior = mat.ior;
    }

    // Emission
    if constexpr (requires { params.emission; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Emission>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, emission));
        params.emission.luminance = float3(mat.luminance);
        if (mat.emission_tex.valid()) {
            params.emission.luminance *= read_tex(mat.emission_tex).xyz;
        }
    }

    // Base
    if constexpr (requires { params.base; })
        if ((float)weight.base > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Base>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, base));
            params.base.color = float3(mat.albedo[0], mat.albedo[1], mat.albedo[2]);
            if (mat.albedo_tex.valid()) {
                auto tex_val = read_tex(mat.albedo_tex);

                params.base.color *= tex_val.xyz;
            }
        }

    // Subsurface
    if constexpr (requires { params.subsurface; })
        if ((float)weight.subsurface > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Subsurface>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, subsurface));
            params.subsurface.color = float3(mat.subsurface_color);
            params.subsurface.radius = float3(mat.subsurface_radius_scale) * (float)mat.subsurface_radius;
            params.subsurface.scatter_anisotropy = mat.subsurface_scatter_anisotropy;
        }

    // Transmission
    if constexpr (requires { params.transmission; })
        if ((float)weight.transmission > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Transmission>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, transmission));
            params.transmission.color = float3(mat.transmission_color);
            params.transmission.depth = mat.transmission_depth;
            params.transmission.scatter = float3(mat.transmission_scatter);
            params.transmission.scatter_anisotropy = mat.transmission_scatter_anisotropy;
            params.transmission.dispersion_scale = mat.transmission_dispersion_scale;
            params.transmission.dispersion_abbe_number = mat.transmission_dispersion_abbe_number;
        }

    // Coat
    if constexpr (requires { params.coat; })
        if ((float)weight.coat > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Coat>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, coat));
            params.coat.coat_onb.rotate_tangent((float)mat.coat_roughness_anisotropy_angle);
            params.coat.color = float3(mat.coat_color);
            params.coat.roughness = mat.coat_roughness;
            params.coat.roughness_anisotropy = mat.coat_roughness_anisotropy;
            params.coat.ior = mat.coat_ior;
            params.coat.darkening = mat.coat_darkening;
            params.coat.roughening = mat.coat_roughening;
        }

    //Fuzz
    if constexpr (requires { params.fuzz; })
        if ((float)weight.fuzz > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Fuzz>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, fuzz));
            params.fuzz.color = float3(mat.fuzz_color);
            params.fuzz.roughness = mat.fuzz_roughness;
        }

    //ThinFilm
    if constexpr (requires { params.thin_film; })
        if ((float)weight.thin_film > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::ThinFilm>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, thin_film));
            params.thin_film.thickness = mat.thin_film_thickness;
            params.thin_film.ior = mat.thin_film_ior;
        }

    // Diffraction
    if constexpr (requires { params.diffraction; })
        if ((float)weight.diffraction > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBR::Diffraction>(mat_type, mat_index * sizeof(OpenPBR) + offsetof(OpenPBR, diffraction));
            params.diffraction.color = float3(mat.diffraction_color);
            params.diffraction.thickness = mat.diffraction_thickness;
            params.diffraction.inv_pitch = float2(mat.diffraction_inv_pitch_x, mat.diffraction_inv_pitch_y);
            params.diffraction.angle = mat.diffraction_angle;
            params.diffraction.lobe_count = mat.diffraction_lobe_count;
            params.diffraction.type = mtl::DiffractionGratingType(mat.diffraction_type);
        }

    return true;
};
}// namespace material