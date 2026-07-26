#pragma once

#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
#else
#include <cstdint>
#endif

namespace fsd {
#ifdef __SHADER_LANG__
using namespace luisa::shader;
#else
using uint = std::uint32_t;
#endif

constexpr uint inverse_cdf_resolution = 1024u;
constexpr uint inverse_cdf_theta_sample_count = inverse_cdf_resolution;
constexpr uint inverse_cdf_radial_sample_count =
    inverse_cdf_resolution * inverse_cdf_resolution;

constexpr uint alpha1_theta_lut_offset = 0u;
constexpr uint alpha1_radial_lut_offset =
    alpha1_theta_lut_offset + inverse_cdf_theta_sample_count;
constexpr uint alpha2_theta_lut_offset =
    alpha1_radial_lut_offset + inverse_cdf_radial_sample_count;
constexpr uint alpha2_radial_lut_offset =
    alpha2_theta_lut_offset + inverse_cdf_theta_sample_count;
constexpr uint inverse_cdf_float_count =
    alpha2_radial_lut_offset + inverse_cdf_radial_sample_count;

#ifdef __SHADER_LANG__
inline float sample_lut_row(
    BindlessBuffer &heap,
    uint lut_index,
    uint offset,
    float sample) {
    auto coordinate = saturate(sample) *
        float(inverse_cdf_resolution - 1u);
    auto lower = min(
        uint(floor(coordinate)),
        inverse_cdf_resolution - 1u);
    auto upper = min(lower + 1u, inverse_cdf_resolution - 1u);
    auto fraction = coordinate - float(lower);
    return lerp(
        heap.uniform_idx_buffer_read<float>(lut_index, offset + lower),
        heap.uniform_idx_buffer_read<float>(lut_index, offset + upper),
        fraction);
}

inline float sample_radial_lut(
    BindlessBuffer &heap,
    uint lut_index,
    uint offset,
    float theta,
    float radial_sample) {
    auto theta_coordinate = saturate(theta * (2.0f * inv_pi)) *
        float(inverse_cdf_resolution - 1u);
    auto lower_row = min(
        uint(floor(theta_coordinate)),
        inverse_cdf_resolution - 1u);
    auto upper_row = min(
        lower_row + 1u,
        inverse_cdf_resolution - 1u);
    auto row_fraction = theta_coordinate - float(lower_row);
    auto lower_radius = sample_lut_row(
        heap,
        lut_index,
        offset + lower_row * inverse_cdf_resolution,
        radial_sample);
    auto upper_radius = sample_lut_row(
        heap,
        lut_index,
        offset + upper_row * inverse_cdf_resolution,
        radial_sample);
    return max(0.0f, lerp(lower_radius, upper_radius, row_fraction));
}

inline float2 sample_canonical_lobe(
    BindlessBuffer &heap,
    uint lut_index,
    bool alpha1_lobe,
    float3 random_sample) {
    auto theta_offset = alpha1_lobe
        ? alpha1_theta_lut_offset
        : alpha2_theta_lut_offset;
    auto radial_offset = alpha1_lobe
        ? alpha1_radial_lut_offset
        : alpha2_radial_lut_offset;
    auto theta = sample_lut_row(
        heap,
        lut_index,
        theta_offset,
        random_sample.x);
    auto radius = sample_radial_lut(
        heap,
        lut_index,
        radial_offset,
        theta,
        random_sample.y);

    auto quadrant = min(uint(random_sample.z * 4.0f), 3u);
    auto signs = float2(
        (quadrant == 0u || quadrant == 3u) ? 1.0f : -1.0f,
        quadrant < 2u ? 1.0f : -1.0f);
    return radius * float2(cos(theta), sin(theta)) * signs;
}
#endif

}// namespace fsd
