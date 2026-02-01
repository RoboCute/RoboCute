// Sky shader with clouds
// Transpiled from GLSL, using PCGSampler instead of texture sampling

#include <sampling/sample_funcs.hpp>
#include <sampling/pcg.hpp>
#include <utils/onb.hpp>
using namespace luisa::shader;

float random(float2 uv) {
    return sampling::PCGSampler(uint2(uv / 64.f)).next();
}

float noise(float2 uv) {
    float2 i = floor(uv);
    float2 f = fract(uv);
    f = f * f * (3.f - 2.f * f);

    float lb = random(i + float2(0.f, 0.f));
    float rb = random(i + float2(1.f, 0.f));
    float lt = random(i + float2(0.f, 1.f));
    float rt = random(i + float2(1.f, 1.f));

    return lerp(lerp(lb, rb, f.x),
                lerp(lt, rt, f.x), f.y);
}

#define OCTAVES 8
float fbm(float2 uv) {
    float value = 0.f;
    float amplitude = .5f;

    for (int i = 0; i < OCTAVES; i++) {
        value += noise(uv) * amplitude;

        amplitude *= .5f;

        uv *= 2.f;
    }

    return value;
}
#define SUN_DIR normalize(float3(.4f, .8f, -.5f))
#define SUN_COLOR (float3(1.f, .9f, .85f) * 10.0f)

float3 Sky(float3 ro, float3 rd, float iTime) {
    const float SC = 1e5f;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y;
    float2 p = (ro + dist * rd).xz;
    p *= 1.2f / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    float3 lightDir = SUN_DIR;
    float sundot = clamp(dot(rd, lightDir), 0.0f, 1.0f);

    float3 cloudCol = float3(1.f);
    float3 skyCol = float3(0.4f, 0.6f, 0.85f) * 1.5f - rd.y * rd.y * 0.5f;
    skyCol = max(skyCol, float3(0.f));
    skyCol = lerp(skyCol, 0.85f * float3(0.7f, 0.75f, 0.85f), pow(1.0f - max(rd.y, 0.0f), 4.0f));

    // sun
    float3 sun = 0.25f * normalize(SUN_COLOR) * pow(sundot, 5.0f);
    sun += 0.25f * float3(1.0f, 0.8f, 0.6f) * pow(sundot, 64.0f);
    sun += 0.2f * float3(1.0f, 0.8f, 0.6f) * pow(sundot, 512.0f);
    skyCol += sun;

    // clouds
    float t = iTime * 0.1f;
    float den = fbm(float2(p.x - t, p.y - t));
    skyCol = lerp(skyCol, cloudCol, smoothstep(.4f, .8f, den));

    // horizon
    skyCol = lerp(skyCol, 0.68f * float3(.418f, .394f, .372f), pow(1.0f - max(rd.y, 0.0f), 16.0f));

    return skyCol;
}

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &img,
    float iTime) {
    float2 fragCoord = float2(dispatch_id().xy);
    float2 iResolution = float2(dispatch_size().xy);

    float2 uv = (fragCoord + 0.5f) / iResolution;
    float2 origin_uv = uv;
    uv -= 0.5f;
    uv.x *= iResolution.x / iResolution.y;

    float3 ro = float3(0.0f, 0.0f, 0.0f);
    float3 rd = sampling::sphere_uv_to_direction(
        float3x3(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1),
        origin_uv);

    float3 col = Sky(ro, rd, iTime);
    if (dot(rd, SUN_DIR) > 0.9999f) {
        col.rgb = SUN_COLOR * 1000.f;
    }
    img.write(dispatch_id().xy, float4(col, 1.0f));
    return 0;
}
