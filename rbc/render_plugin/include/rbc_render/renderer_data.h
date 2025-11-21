#pragma once
#include <rbc_core/type_info.h>
#include <luisa/core/mathematics.h>
namespace rbc {
using namespace luisa;
struct CameraData {
    float4x4 view;
    float4x4 proj;
    float4x4 vp;
    float4x4 inv_view;
    float4x4 inv_sky_view;
    float4x4 inv_proj;
    float4x4 inv_vp;
    float3x3 world_to_sky;

    float4x4 last_view;
    float4x4 last_sky_view;
    float4x4 last_proj;
    float4x4 last_vp;
    float4x4 last_sky_vp;
    float4x4 last_inv_vp;
};
struct JitterData {
    float2 last_jitter;
    uint jitter_phase_count;
    float2 jitter;
};
struct RealtimePTSettings {
    bool split_albedo_mode{false};
};
}// namespace rbc
RBC_RTTI(rbc::CameraData)
RBC_RTTI(rbc::JitterData)
RBC_RTTI(rbc::RealtimePTSettings)