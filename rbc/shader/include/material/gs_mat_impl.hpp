#pragma once
#include <luisa/std.hpp>
#include <material/mat_codes.hpp>
#include <material/openpbr_params.hpp>
#include <virtual_tex/stream.hpp>
#include "gs_mat.hpp"
#include <geometry/procedural_types.hpp>

namespace material {
/// Get the number of floats per Gaussian for given SH degree
[[nodiscard]] static uint sh_num_floats(uint sh_degree) {
    return (sh_degree + 1) * (sh_degree + 1) * 3;
}
/// Get the size in bytes for given SH degree
[[nodiscard]] static uint sh_size_bytes(uint sh_degree) {
    return sh_num_floats(sh_degree) * sizeof(float);
}

inline bool OpenPBRParticle::transform_to_params(
    BindlessBuffer &buffer_heap,
    BindlessImage &image_heap,
    uint procedural_type,
    uint mat_heap_index,
    uint mat_index,
    uint mat_byte_offset,
    uint sh_degree,
    auto &params,
    float3 input_dir,
    bool &reject,
    float3 world_pos,
    auto &&...) {
    if (procedural_type == geometry::ProceduralTypeID::GaussianSplat) {
        if constexpr (requires { params.emission; }) {
            auto byte_offset = mat_byte_offset + mat_index * sh_size_bytes(sh_degree);
            auto num_coeffs = sh_num_floats(sh_degree);
            // Read SH coefficients from buffer: 3 floats per coefficient (RGB)
            // for bands 0 to sh_degree
            auto sh0 = buffer_heap.uniform_idx_byte_buffer_read<float3>(
                mat_heap_index, byte_offset + 0);
            float3 luminance = sh0 * 0.2820947917f;// DC component (band 0)
            if (sh_degree >= 1) {
                // Band 1: 3 coefficients (RGB) x 3 basis functions = 9 floats
                auto sh1_0 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 1 * 12);
                auto sh1_1 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 2 * 12);
                auto sh1_2 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 3 * 12);
                float3 dir = input_dir;
                luminance += float3(sh1_0) * (0.4886025119f * dir.y);
                luminance += float3(sh1_1) * (0.4886025119f * dir.z);
                luminance += float3(sh1_2) * (0.4886025119f * dir.x);
            }

            if (sh_degree >= 2) {
                // Band 2: 3 coefficients (RGB) x 5 basis functions = 15 floats
                auto sh2_0 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 4 * 12);
                auto sh2_1 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 5 * 12);
                auto sh2_2 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 6 * 12);
                auto sh2_3 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 7 * 12);
                auto sh2_4 = buffer_heap.uniform_idx_byte_buffer_read<std::array<float, 3>>(
                    mat_heap_index, byte_offset + 8 * 12);
                float3 dir = input_dir;
                luminance += float3(sh2_0) * (1.0925484306f * dir.x * dir.y);
                luminance += float3(sh2_1) * (1.0925484306f * dir.y * dir.z);
                luminance += float3(sh2_2) * (0.3153915652f * (3 * dir.z * dir.z - 1));
                luminance += float3(sh2_3) * (1.0925484306f * dir.x * dir.z);
                luminance += float3(sh2_4) * (0.5462742153f * (dir.x * dir.x - dir.y * dir.y));
            }

            params.emission.luminance = luminance;
        }
        return false;
    }
    // Weight
    if constexpr (requires { params.weight; }) {
        auto weight = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Weight>(
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
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
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, specular));
        params.specular.color = float3(mat.specular_color);
        params.specular.roughness = mat.roughness;
        params.specular.roughness_anisotropy = mat.roughness_anisotropy;
        params.specular.ior = mat.ior;
    }

    // Emission
    if constexpr (requires { params.emission; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Emission>(
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, emission));
        params.emission.luminance = float3(mat.luminance);
    }

    // Base - OpenPBRParticle doesn't have a base color, use default white
    if constexpr (requires { params.base; }) {
        auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Base>(
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, base));
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
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
        if (weight.transmission > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Transmission>(
                mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, transmission));
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
            mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, weight));
        if (weight.coat > 0.f) {
            auto mat = buffer_heap.uniform_idx_byte_buffer_read<OpenPBRParticle::Coat>(
                mat_heap_index, mat_byte_offset + mat_index * sizeof(OpenPBRParticle) + offsetof(OpenPBRParticle, coat));
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
