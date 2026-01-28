#include "gen_common.hpp"
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &volume) {
    float2 vUV = float2(dispatch_id().xy) / float2(dispatch_size().xy);
    float3 coord = fract(float3(vUV + float2(.2, 0.62), .5));

    float4 col = float4(1);

    float mfbm = 0.9;
    float mvor = 0.7;

    col.r = lerp(1.f, tilableFbm(coord, 7, 4.), mfbm) *
            lerp(1.f, tilableVoronoi(coord, 8, 9.), mvor);
    col.g = 0.625 * tilableVoronoi(coord + 0.f, 3, 15.f) +
            0.250 * tilableVoronoi(coord + 0.f, 3, 19.f) +
            0.125 * tilableVoronoi(coord + 0.f, 3, 23.f) - 1.f;
    col.b = 1. - tilableVoronoi(coord + 0.5f, 6, 9.);
    volume.write(dispatch_id().xy, col);
    return 0;
}