#pragma once

#define NO_RESTIR_DI
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
    float3x3 world_2_sky_mat;
    
    float3 cam_pos;
    
    // 8-byte aligned (float2)
    float2 jitter_offset;
    float2 tex_grad_scale;// render_size / display_size
    
    // 4-byte aligned (float/uint/bool)
    float focus_distance;
    float lens_radius;
    float gbuffer_temporal_weight;
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
};
struct MultiBouncePixel {
    std::array<float, 3> beta;
    uint pixel_id;
    std::array<float, 3> input_pos;
    float pdf_bsdf;
    std::array<float, 3> input_dir;
    float length_sum;
};
}// namespace offline
