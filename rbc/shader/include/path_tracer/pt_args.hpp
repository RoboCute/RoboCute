#pragma once

#define NO_RESTIR_DI
#include <spectrum/spectrum_args.hpp>
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
#endif

inline namespace offline {

struct PTArgs {
    // 64-byte aligned (float4x4)
    float4x4 inv_view;
    float4x4 view;
    float4x4 inv_vp;
    
    float3x3 resource_to_rec2020_mat;
    SpectrumAccumulationArgs spectrum;
    float3x3 world_2_sky_mat;
    
    float3 cam_pos;
    
    // 8-byte aligned (float2)
    float2 jitter_offset;
    float2 tex_grad_scale;// render_size / display_size
    
    // 4-byte aligned (float/uint/bool)
    float focus_distance;
    float lens_radius;
    uint sky_heap_idx;
    uint alias_table_idx;
    uint pdf_table_idx;
    uint frame_index;
    uint frame_countdown;
    uint light_count;
    uint bounce;
    uint geometry_mask;
    bool enable_physical_camera;
    bool reset_emission;
    bool require_reject;
    bool write_id_map;
    bool probe_initial_medium;
};
struct MultiBouncePixel {
    std::array<float, 3> beta;
    uint pixel_id;
    std::array<float, 3> input_pos;
    float pdf_bsdf;
    std::array<float, 3> input_dir;
    uint spectrum_state;
};

constexpr uint SPECTRUM_SAMPLE_BITS = 24u;
constexpr uint SPECTRUM_SAMPLE_MASK = (1u << SPECTRUM_SAMPLE_BITS) - 1u;

constexpr uint pack_spectrum_state(
    float wavelength_sample,
    uint hero_index,
    bool selected_wavelength,
    uint volume_count) {
    uint quantized_sample = uint(wavelength_sample * float(1u << SPECTRUM_SAMPLE_BITS));
    quantized_sample = quantized_sample < SPECTRUM_SAMPLE_MASK
                           ? quantized_sample
                           : SPECTRUM_SAMPLE_MASK;
    return quantized_sample |
           ((hero_index & 3u) << 24u) |
           (uint(selected_wavelength) << 26u) |
           ((volume_count & 3u) << 27u);
}
constexpr float unpack_wavelength_sample(uint state) {
    return float(state & SPECTRUM_SAMPLE_MASK) / float(1u << SPECTRUM_SAMPLE_BITS);
}
constexpr uint unpack_hero_index(uint state) { return (state >> 24u) & 3u; }
constexpr bool unpack_selected_wavelength(uint state) { return ((state >> 26u) & 1u) != 0u; }
constexpr uint unpack_volume_count(uint state) { return (state >> 27u) & 3u; }
}// namespace offline
