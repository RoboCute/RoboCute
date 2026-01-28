#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;

template<concepts::float_family T>
T mod(T a, T b) {
    return a - b * floor(a / b);
}

float hash13(float3 p3) {
    p3 = fract(p3 * 1031.1031f);
    p3 += dot(p3, p3.yzx + 19.19f);
    return fract((p3.x + p3.y) * p3.z);
}

float voronoi(float3 x, float tile) {
    float3 p = floor(x);
    float3 f = fract(x);

    float res = 100.;
    for (int k = -1; k <= 1; k++) {
        for (int j = -1; j <= 1; j++) {
            for (int i = -1; i <= 1; i++) {
                float3 b = float3(i, j, k);
                float3 c = p + b;

                if (tile > 0.) {
                    c = mod(c, float3(tile));
                }

                float3 r = float3(b) - f + hash13(c);
                float d = dot(r, r);

                if (d < res) {
                    res = d;
                }
            }
        }
    }

    return 1. - res;
}
float valueHash(float3 p3) {
    p3 = fract(p3 * 0.1031f);
    p3 += dot(p3, p3.yzx + 19.19f);
    return fract((p3.x + p3.y) * p3.z);
}

//
// Noise functions used for cloud shapes
//
float valueNoise(float3 x, float tile) {
    float3 p = floor(x);
    float3 f = fract(x);
    f = f * f * (3.0f - 2.0f * f);

    return lerp(lerp(lerp(valueHash(mod(p + float3(0, 0, 0), float3(tile))),
                          valueHash(mod(p + float3(1, 0, 0), float3(tile))), f.x),
                     lerp(valueHash(mod(p + float3(0, 1, 0), float3(tile))),
                          valueHash(mod(p + float3(1, 1, 0), float3(tile))), f.x),
                     f.y),
                lerp(lerp(valueHash(mod(p + float3(0, 0, 1), float3(tile))),
                          valueHash(mod(p + float3(1, 0, 1), float3(tile))), f.x),
                     lerp(valueHash(mod(p + float3(0, 1, 1), float3(tile))),
                          valueHash(mod(p + float3(1, 1, 1), float3(tile))), f.x),
                     f.y),
                f.z);
}
float tilableVoronoi(float3 p, const int octaves, float tile) {
    float f = 1.;
    float a = 1.;
    float c = 0.;
    float w = 0.;

    if (tile > 0.) f = tile;

    for (int i = 0; i < octaves; i++) {
        c += a * voronoi(p * f, f);
        f *= 2.0;
        w += a;
        a *= 0.5;
    }

    return c / w;
}

float tilableFbm(float3 p, const int octaves, float tile) {
    float f = 1.;
    float a = 1.;
    float c = 0.;
    float w = 0.;

    if (tile > 0.) f = tile;

    for (int i = 0; i < octaves; i++) {
        c += a * valueNoise(p * f, f);
        f *= 2.0;
        w += a;
        a *= 0.5;
    }

    return c / w;
}