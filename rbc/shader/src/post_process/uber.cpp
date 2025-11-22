// #define DEBUG
#include <luisa/printer.hpp>
#include <luisa/std.hpp>
#include <spectrum/color_space.hpp>
#include <post_process/local_exposure.hpp>
#include <post_process/colors.hpp>
using namespace luisa::shader;
#include <post_process/uber_arg.inl>
#include <post_process/lpm.hpp>

// #define DISABLE_POST_PROCESS
template<concepts::float_family T>
T make_safe_divisor(T t) {
    return ite(abs(t) < 1e-5f, ite(t > T(0.0f), T(1e-5f), T(-1e-5f)), t);
}
static float2 distort(float2 uv, float4 distortion_Amount, float4 distortion_CenterScale) {
    if (abs(distortion_Amount.w) < 0.1) {
        return uv;
    }
    uv = (uv - float2(0.5)) * distortion_Amount.z + float2(0.5);
    float2 ruv = distortion_CenterScale.zw * (uv - float2(0.5) - distortion_CenterScale.xy);
    float ru = length(float2(ruv));

    if (distortion_Amount.w > 0.0) {
        float wu = ru * distortion_Amount.x;
        ru = tan(wu) * (1.0 / (ru * distortion_Amount.y));
        uv = uv + ruv * float2(ru - 1.0);
    } else {
        ru = (1.0 / ru) * distortion_Amount.x * atan(ru * distortion_Amount.y);
        uv = uv + ruv * float2(ru - 1.0);
    }
    // if (all(dispatch_id().xy  == uint2(float2(dispatch_size().xy) * 0.1f))){
    //     device_log("{}, {}, {}", ruv, uv, ru);
    // }
    return uv;
}
#define MAX_CHROMATIC_SAMPLES 16
[[kernel_2d(16, 8)]] int kernel(
    SampleImage &src_img,
    SampleVolume &tonemap_volume,
    Args args,
    LpmArgs lpm_args,
    Buffer<float> &exposure_buffer,
    Image<float> &result) {
    auto id = dispatch_id().xy;
    float2 uv = (float2(id) + 0.5f) / float2(dispatch_size().xy);
#ifndef DISABLE_POST_PROCESS
    uv = distort(uv, args.distortion_Amount, args.distortion_CenterScale);
#endif
    float4 tex_val = src_img.sample(uv, Filter::POINT, Address::EDGE);
    float3 col = tex_val.xyz;
#ifndef DISABLE_POST_PROCESS
    if (args.chromatic_aberration > 1e-5f) {
        float2 coords = 2.0f * uv - 1.0f;
        float2 end = uv - coords * dot(coords, coords) * args.chromatic_aberration;

        float2 diff = end - uv;
        int samples = clamp(int(length(float2(dispatch_size().xy) * diff / float2(2.0))), 3, MAX_CHROMATIC_SAMPLES);
        float2 delta = diff / float(samples);
        float2 pos = uv;
        float3 sum(0);
        float3 filterSum(0);

        for (int i = 0; i < samples; i++) {
            float t = (i + 0.5) / samples;
            float3 s = src_img.sample(distort(pos, args.distortion_Amount, args.distortion_CenterScale), Filter::LINEAR_POINT, Address::EDGE).xyz;
            float3 lut_filter(1.0f);
            // TODO: do we need lut ?
            if (t < 0.5f) {
                lut_filter = lerp(float3(1, 0, 0), float3(0, 1, 0), t * 2.0f);
            } else {
                lut_filter = lerp(float3(0, 1, 0), float3(0, 0, 1), (t - 0.5f) * 2.0f);
            }

            // float4 s = SAMPLE_TEXTURE2D_LOD(_MainTex, sampler_MainTex, UnityStereoTransformScreenSpaceTex(Distort(pos)), 0);
            // float4 filter = float4(SAMPLE_TEXTURE2D_LOD(_ChromaticAberration_SpectralLut, sampler_ChromaticAberration_SpectralLut, float2(t, 0.0), 0).rgb, 1.0);

            sum += s * lut_filter;
            filterSum += lut_filter;
            pos += delta;
        }

        col = sum / filterSum;
    }
    col *= exposure_buffer.read(0);
    /////// Bloom
    // if (args.bloom_settings.y > 1e-4f) {
    // 	float4 bloom = UpsampleTent(bloom_img, uv, float2(1) / float2(bloom_img.size()), float4(args.bloom_settings.x));
    // 	float4 dirt(0);
    // 	// if (args.bloom_dirty_tex_idx != max_uint32) {
    // 	// 	dirt = heap.image_sample(args.bloom_dirty_tex_idx, uv * args.bloom_dirt_tile_offset.xy + args.bloom_dirt_tile_offset.zw);
    // 	// }
    // 	bloom *= args.bloom_settings.y;
    // 	dirt *= args.bloom_settings.z;
    // 	col += (bloom * args.bloom_color).xyz;
    // 	col += (dirt * bloom).xyz;
    // }

    /////// Tonemap
    col *= args.hdr_input_multiplier;
    col = tonemap_volume.sample(LUT_SPACE_ENCODE(col), Filter::LINEAR_POINT, Address::EDGE).xyz;
    {
        uint flags = lpm_args.flags;
        bool shoulder = (lpm_args.flags & 1) == 1;
        lpm_args.flags >>= 1;
        bool con = (lpm_args.flags & 1) == 1;
        lpm_args.flags >>= 1;
        bool soft = (lpm_args.flags & 1) == 1;
        lpm_args.flags >>= 1;
        bool con2 = (lpm_args.flags & 1) == 1;
        lpm_args.flags >>= 1;
        bool clip = (lpm_args.flags & 1) == 1;
        lpm_args.flags >>= 1;
        bool scaleOnly = (lpm_args.flags & 1) == 1;
        LpmFilter(
            lpm_args.ctl,
            col.x,
            col.y,
            col.z,
            shoulder,
            con,
            soft,
            con2,
            clip,
            scaleOnly);
    }
    col = pow(col, args.gamma);
    if (args.use_hdr10) {
        col *= 10000.0f / 80.0f;
    }
#endif
    if (args.saturate_result) {
        col = saturate(col);
    }
    result.write(id + args.pixel_offset, float4(col, tex_val.w));
    return 0;
}