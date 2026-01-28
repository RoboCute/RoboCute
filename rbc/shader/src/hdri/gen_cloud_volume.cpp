#include "gen_common.hpp"
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &volume) {
    auto fragCoord = float2(dispatch_id().xy);
    // pack 32x32x32 3d texture in 2d texture (32x8, 32x4) (with padding)
    float z = floor(fragCoord.x / 34.f) + 8.f * floor(fragCoord.y / 34.f);
    float2 uv = mod(fragCoord.xy, float2(34.f)) - 1.f;
    float3 coord = float3(uv, z) / 32.f;

    float r = tilableVoronoi(coord, 16, 3.);
    float g = tilableVoronoi(coord, 4, 8.);
    float b = tilableVoronoi(coord, 4, 16.);

    float c = max(0., 1. - (r + g * .5 + b * .25) / 1.75);

    auto fragColor = float4(c, c, c, c);
    volume.write(dispatch_id().xy, fragColor * 0.5f);
    return 0;
}