#include "gen_common.hpp"
#include <sampling/sample_funcs.hpp>
#include <sampling/pcg.hpp>
using namespace luisa::shader;
#define CLOUD_MARCH_STEPS 128
#define CLOUD_SELF_SHADOW_STEPS 16
#define INV_SCENE_SCALE (.1f)
#define EARTH_RADIUS (1500000.f)// (6371000.)
#define CLOUDS_BOTTOM (1350.f)
#define CLOUDS_TOP (2350.f)

#define CLOUDS_LAYER_BOTTOM (-150.f)
#define CLOUDS_LAYER_TOP (-70.f)

#define CLOUDS_COVERAGE (.52f)
#define CLOUDS_LAYER_COVERAGE (.41f)

#define CLOUDS_DETAIL_STRENGTH (.225f)
#define CLOUDS_BASE_EDGE_SOFTNESS (.1f)
#define CLOUDS_BOTTOM_SOFTNESS (.25f)
#define CLOUDS_DENSITY (.03f)
#define CLOUDS_SHADOW_MARGE_STEP_SIZE (10.f)
#define CLOUDS_LAYER_SHADOW_MARGE_STEP_SIZE (4.f)
#define CLOUDS_SHADOW_MARGE_STEP_MULTIPLY (1.3f)
#define CLOUDS_FORWARD_SCATTERING_G (.8f)
#define CLOUDS_BACKWARD_SCATTERING_G (-.2f)
#define CLOUDS_SCATTERING_LERP (.5f)

#define CLOUDS_AMBIENT_COLOR_TOP (float3(149., 167., 200.) * (1.5f / 255.f))
#define CLOUDS_AMBIENT_COLOR_BOTTOM (float3(39., 67., 87.) * (1.5f / 255.f))
#define CLOUDS_MIN_TRANSMITTANCE .1f

#define CLOUDS_BASE_SCALE 1.51f
#define CLOUDS_DETAIL_SCALE 20.f
#define SUN_DIR normalize(float3(-.3f, .8f, .5f))
#define SCENE_SCALE 1.f
#define SUN_COLOR (float3(1.f, .9f, .85f) * 10.0f)
#define HEIGHT_BASED_FOG_C 0.05f
#define HEIGHT_BASED_FOG_B 0.02f

float3 getSkyColor(float3 rd) {
    rd.y = max(rd.y, 0.f);
    float sundot = clamp(dot(rd, SUN_DIR), 0.0F, 1.0F);
    float3 col = float3(0.4, 0.6, 0.85) * 1.5f - max(rd.y, 0.01f) * max(rd.y, 0.01F) * 0.5f;
    col = max(col, float3(0.f));
    col = lerp(col, 0.85f * float3(0.7, 0.75, 0.85), pow((1.0f - rd.y), 6.0f));

    col += 0.25f * float3(1.0, 0.7, 0.4) * pow(sundot, 5.0f);
    col += 0.25f * float3(1.0, 0.8, 0.6) * pow(sundot, 64.0f);
    col += 0.20f * float3(1.0, 0.8, 0.6) * pow(sundot, 512.0f);

    col += clamp((0.1f - rd.y) * 10.f, 0.f, 1.f) * float3(.0, .1, .2);
    col += 0.2f * float3(1.0, 0.8, 0.6) * pow(sundot, 8.0f);
    return col;
}

float HenyeyGreenstein(float sundotrd, float g) {
    float gg = g * g;
    return (1. - gg) / pow(1. + gg - 2. * g * sundotrd, 1.5);
}

float interectCloudSphere(float3 rd, float r) {
    float b = EARTH_RADIUS * rd.y;
    float d = b * b + r * r + 2. * EARTH_RADIUS * r;
    return -b + sqrt(d);
}

float linearstep(const float s, const float e, float v) {
    return clamp((v - s) * (1. / (e - s)), 0., 1.);
}

float linearstep0(const float e, float v) {
    return min(v * (1. / e), 1.);
}

float remap(float v, float s, float e) {
    return (v - s) / (e - s);
}

float cloudMapBase(SampleImage &iChannel0, float3 p, float norY) {
    float3 uv = p * (0.00005f * CLOUDS_BASE_SCALE);
    float3 cloud = iChannel0.sample(uv.xz, Filter::LINEAR_POINT, Address::REPEAT).xyz;

    float n = norY * norY;
    n *= cloud.b;
    n += pow(1. - norY, 16.);
    return remap(cloud.r - n, cloud.g, 1.);
}

float cloudMapDetail(SampleImage &iChannel3, float3 p) {
    // 3d lookup 2d texture :(
    p = abs(p) * (0.0016f * CLOUDS_BASE_SCALE * CLOUDS_DETAIL_SCALE);

    float yi = mod(p.y, 32.f);
    int2 offset = int2(mod(yi, 8.f), mod(floor(yi / 8.), 4.)) * 34 + 1;
    float a = iChannel3.sample(mod(p.xz, float2(32.f)) + float2(offset.xy) + 1.f / float2(dispatch_size().xy), Filter::LINEAR_POINT, Address::REPEAT).x;

    yi = mod(p.y + 1.0f, 32.f);
    offset = int2(mod(yi, 8.f), mod(floor(yi / 8.), 4.)) * 34 + 1;
    float b = iChannel3.sample((mod(p.xz, float2(32.f)) + float2(offset.xy) + 1.f) / float2(dispatch_size().xy), Filter::LINEAR_POINT, Address::REPEAT).x;

    return lerp(a, b, fract(p.y));
}

float cloudGradient(float norY) {
    return linearstep(0., .05, norY) - linearstep(.8, 1.2, norY);
}

float cloudMap(SampleImage &iChannel0, SampleImage &iChannel3, float3 pos, float3 rd, float norY) {
    float3 ps = pos;

    float m = cloudMapBase(iChannel0, ps, norY);
    m *= cloudGradient(norY);

    float dstrength = smoothstep(1.f, 0.5f, m);

    // erode with detail
    if (dstrength > 0.) {
        m -= cloudMapDetail(iChannel3, ps) * dstrength * CLOUDS_DETAIL_STRENGTH;
    }

    m = smoothstep(0.f, CLOUDS_BASE_EDGE_SOFTNESS, m + (CLOUDS_COVERAGE - 1.f));
    m *= linearstep0(CLOUDS_BOTTOM_SOFTNESS, norY);

    return clamp(m * CLOUDS_DENSITY * (1. + max((ps.x - 7000.) * 0.005, 0.)), 0., 1.);
}

float volumetricShadow(SampleImage &iChannel0, SampleImage &iChannel3, float3 from, float sundotrd,
                       sampling::PCGSampler &sampler) {
    float dd = CLOUDS_SHADOW_MARGE_STEP_SIZE;
    float3 rd = SUN_DIR;
    float d = dd * .5;
    float shadow = 1.0;

    for (int s = 0; s < CLOUD_SELF_SHADOW_STEPS; s++) {
        float3 pos = from + rd * (d + dd * (sampler.next() - 0.5f));
        float norY = (length(pos) - (EARTH_RADIUS + CLOUDS_BOTTOM)) * (1. / (CLOUDS_TOP - CLOUDS_BOTTOM));

        if (norY > 1.) return shadow;

        float muE = cloudMap(iChannel0, iChannel3, pos, rd, norY);
        shadow *= exp(-muE * dd);

        dd *= CLOUDS_SHADOW_MARGE_STEP_MULTIPLY;
        d += dd;
    }
    return shadow;
}

float4 renderClouds(SampleImage &iChannel0, SampleImage &iChannel3, float3 ro, float3 rd, float &dist) {
    if (rd.y < 0.) {
        return float4(0, 0, 0, 10);
    }

    ro.xz *= SCENE_SCALE;
    ro.y = sqrt(EARTH_RADIUS * EARTH_RADIUS - dot(ro.xz, ro.xz));

    float start = interectCloudSphere(rd, CLOUDS_BOTTOM);
    float end = interectCloudSphere(rd, CLOUDS_TOP);

    if (start > dist) {
        return float4(0, 0, 0, 10);
    }

    end = min(end, dist);

    float sundotrd = dot(rd, -SUN_DIR);

    // raymarch
    float d = start;
    float dD = (end - start) / float(CLOUD_MARCH_STEPS);

    float h = hash13(rd);
    d -= dD * h;

    float scattering = lerp(HenyeyGreenstein(sundotrd, CLOUDS_FORWARD_SCATTERING_G),
                            HenyeyGreenstein(sundotrd, CLOUDS_BACKWARD_SCATTERING_G), CLOUDS_SCATTERING_LERP);

    float transmittance = 1.0;
    float3 scatteredLight = float3(0.0, 0.0, 0.0);

    dist = EARTH_RADIUS;
    sampling::PCGSampler sampler(dispatch_id().xy);
    for (int s = 0; s < CLOUD_MARCH_STEPS; s++) {
        float3 p = ro + (d + dD * (sampler.next() - 0.5f)) * rd;

        float norY = clamp((length(p) - (EARTH_RADIUS + CLOUDS_BOTTOM)) * (1. / (CLOUDS_TOP - CLOUDS_BOTTOM)), 0., 1.);

        float alpha = cloudMap(iChannel0, iChannel3, p, rd, norY);

        if (alpha > 0.) {
            dist = min(dist, d);
            float3 ambientLight = lerp(CLOUDS_AMBIENT_COLOR_BOTTOM, CLOUDS_AMBIENT_COLOR_TOP, norY);

            float3 S = (ambientLight + SUN_COLOR * (scattering * volumetricShadow(iChannel0, iChannel3, p, sundotrd, sampler))) * alpha;
            float dTrans = exp(-alpha * dD);
            float3 Sint = (S - S * dTrans) * (1.f / alpha);
            scatteredLight += transmittance * Sint;
            transmittance *= dTrans;
        }

        if (transmittance <= CLOUDS_MIN_TRANSMITTANCE) break;
        d += dD;
    }

    return float4(scatteredLight, transmittance);
}

//
//
// !Because I wanted a second cloud layer (below the horizon), I copy-pasted
// almost all of the code above:
//

float cloudMapLayer(SampleImage &iChannel0, SampleImage &iChannel3, float3 pos, float3 rd, float norY) {
    float3 ps = pos;

    float m = cloudMapBase(iChannel0, ps, norY);
    // m *= cloudGradient( norY );
    float dstrength = smoothstep(1.f, 0.5f, m);

    // erode with detail
    if (dstrength > 0.) {
        m -= cloudMapDetail(iChannel3, ps) * dstrength * CLOUDS_DETAIL_STRENGTH;
    }

    m = smoothstep(0.f, CLOUDS_BASE_EDGE_SOFTNESS, m + (CLOUDS_LAYER_COVERAGE - 1.f));

    return clamp(m * CLOUDS_DENSITY, 0.f, 1.f);
}

float volumetricShadowLayer(SampleImage &iChannel0, SampleImage &iChannel3, float3 from, float sundotrd) {
    float dd = CLOUDS_LAYER_SHADOW_MARGE_STEP_SIZE;
    float3 rd = SUN_DIR;
    float d = dd * .5;
    float shadow = 1.0;

    for (int s = 0; s < CLOUD_SELF_SHADOW_STEPS; s++) {
        float3 pos = from + rd * d;
        float norY = clamp((pos.y - CLOUDS_LAYER_BOTTOM) * (1. / (CLOUDS_LAYER_TOP - CLOUDS_LAYER_BOTTOM)), 0., 1.);

        if (norY > 1.) return shadow;

        float muE = cloudMapLayer(iChannel0, iChannel3, pos, rd, norY);
        shadow *= exp(-muE * dd);

        dd *= CLOUDS_SHADOW_MARGE_STEP_MULTIPLY;
        d += dd;
    }
    return shadow;
}

float4 renderCloudLayer(SampleImage &iChannel0, SampleImage &iChannel3, float3 ro, float3 rd, float &dist) {
    if (rd.y > 0.) {
        return float4(0, 0, 0, 10);
    }

    ro.xz *= SCENE_SCALE;
    ro.y = 0.;

    float start = CLOUDS_LAYER_TOP / rd.y;
    float end = CLOUDS_LAYER_BOTTOM / rd.y;

    if (start > dist) {
        return float4(0, 0, 0, 10);
    }

    end = min(end, dist);

    float sundotrd = dot(rd, -SUN_DIR);

    // raymarch
    float d = start;
    float dD = (end - start) / float(CLOUD_MARCH_STEPS);

    float h = hash13(rd);
    d -= dD * h;

    float scattering = lerp(HenyeyGreenstein(sundotrd, CLOUDS_FORWARD_SCATTERING_G), HenyeyGreenstein(sundotrd, CLOUDS_BACKWARD_SCATTERING_G), CLOUDS_SCATTERING_LERP);

    float transmittance = 1.0;
    float3 scatteredLight = float3(0.0, 0.0, 0.0);

    dist = EARTH_RADIUS;

    for (int s = 0; s < CLOUD_MARCH_STEPS; s++) {
        float3 p = ro + d * rd;

        float norY = clamp((p.y - CLOUDS_LAYER_BOTTOM) * (1. / (CLOUDS_LAYER_TOP - CLOUDS_LAYER_BOTTOM)), 0., 1.);

        float alpha = cloudMapLayer(iChannel0, iChannel3, p, rd, norY);

        if (alpha > 0.) {
            dist = min(dist, d);
            float3 ambientLight = lerp(CLOUDS_AMBIENT_COLOR_BOTTOM, CLOUDS_AMBIENT_COLOR_TOP, norY);

            float3 S = .7f * (ambientLight + SUN_COLOR * (scattering * volumetricShadowLayer(iChannel0, iChannel3, p, sundotrd))) * alpha;
            float dTrans = exp(-alpha * dD);
            float3 Sint = (S - S * dTrans) * (1.f / alpha);
            scatteredLight += transmittance * Sint;
            transmittance *= dTrans;
        }

        if (transmittance <= CLOUDS_MIN_TRANSMITTANCE) break;

        d += dD;
    }

    return float4(scatteredLight, transmittance);
}

[[kernel_2d(16, 8)]] int kernel(
    SampleImage &iChannel0, SampleImage &iChannel3,
    Image<float> &img) {
    float2 fragCoord = float2(dispatch_id().xy);
    int2 iResolution = int2(dispatch_size().xy);
    float dist = EARTH_RADIUS;
    float4 col = float4(0, 0, 0, 0);

    float3 ro = float3(0);
    float3 rd = sampling::sphere_uv_to_direction(
        float3x3(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1),
        (fragCoord + 0.5f) / float2(iResolution));
    rd = normalize(rd);
    if (rd.y > 0.) {
        // clouds
        col = renderClouds(iChannel0, iChannel3, ro, rd, dist);
        col.rgb = max(col.rgb, float3(0.f));
        col.rgb = lerp(col.rgb, getSkyColor(rd), saturate(exp(col.a) / (1.f + exp(col.a))));
    } else{
        col.xyz = getSkyColor(rd);
    }

    // sun
    if (dot(rd, SUN_DIR) > 0.9999f) {
        col.rgb = SUN_COLOR * 2000.f;
    }
    img.write(dispatch_id().xy, col);
    return 0;
}