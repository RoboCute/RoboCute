#pragma once
#include <luisa/std.hpp>
#include <material/mat_codes.hpp>
#include <material/openpbr_params.hpp>
#include <virtual_tex/stream.hpp>
#include "gs_mat.hpp"

namespace material {

inline bool OpenPBRParticle::transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    uint mat_type,
    uint mat_index,
    uint mat_byte_offset,
    auto &params,
    float3 input_dir,
    bool &reject,
    float3 world_pos,
    auto &&...) {

    // Weight
    if constexpr (requires { params.weight; }) {
        auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Weight>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
        params.weight.base = 1.0;
        params.weight.diffuse_roughness = weight.diffuse_roughness;
        params.weight.specular = weight.specular;
        params.weight.metalness = weight.metallic;
        params.weight.transmission = weight.transmission;
        params.weight.coat = weight.coat;
        // OpenPBRParticle doesn't have subsurface, fuzz, thin_film, diffraction weights
        params.weight.subsurface = 0.0f;
        params.weight.fuzz = 0.0f;
        params.weight.thin_film = 0.0f;
        params.weight.diffraction = 0.0f;
    }

    // Specular
    if constexpr (requires { params.specular; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Specular>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, specular));
        params.specular.color = float3(mat.specular_color);
        params.specular.roughness = mat.roughness;
        params.specular.roughness_anisotropy = mat.roughness_anisotropy;
        params.specular.ior = mat.ior;
    }

    // Emission
    if constexpr (requires { params.emission; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Emission>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, emission));
        params.emission.luminance = float3(mat.luminance);
    }

    // Base - OpenPBRParticle doesn't have a base color, use default white
    if constexpr (requires { params.base; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Base>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, base));
        params.base.color = float3(mat.albedo);
    }

    // Subsurface - OpenPBRParticle doesn't have subsurface data
    if constexpr (requires { params.subsurface; }) {
        params.subsurface.color = float3(0.0f);
        params.subsurface.radius = float3(0.0f);
        params.subsurface.scatter_anisotropy = 0.0f;
    }

    // Transmission
    if constexpr (requires { params.transmission; }) {
        auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Weight>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
        if (weight.transmission > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Transmission>(
                mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, transmission));
            params.transmission.color = float3(mat.transmission_color);
            params.transmission.depth = mat.transmission_depth;
            params.transmission.scatter = float3(mat.transmission_scatter);
            params.transmission.scatter_anisotropy = mat.transmission_scatter_anisotropy;
            params.transmission.dispersion_scale = mat.transmission_dispersion_scale;
            params.transmission.dispersion_abbe_number = mat.transmission_dispersion_abbe_number;
        }
    }

    // Coat
    if constexpr (requires { params.coat; }) {
        auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Weight>(
            mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
        if (weight.coat > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Coat>(
                mat_type, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, coat));
            params.coat.color = float3(mat.coat_color);
            params.coat.roughness = mat.coat_roughness;
            params.coat.roughness_anisotropy = mat.coat_roughness_anisotropy;
            params.coat.ior = mat.coat_ior;
            params.coat.darkening = mat.coat_darkening;
            params.coat.roughening = mat.coat_roughening;
        }
    }

    return true;
}

}// namespace material
