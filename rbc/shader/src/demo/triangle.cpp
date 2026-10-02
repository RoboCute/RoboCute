#include <luisa/std.hpp>
using namespace luisa::shader;

// Minimal triangle rasterization demo (compute shader).
// Each pixel computes its barycentric coordinates against a fixed
// triangle in NDC space and writes the interpolated vertex color
// inside the triangle, dark gray outside.

[[kernel_2d(16, 8)]] int kernel(
    Image<float>& img) {
    auto coord = dispatch_id().xy;
    auto size = dispatch_size().xy;
    // Pixel center in NDC space [-1, 1], y pointing up
    float2 p = (float2(coord) + 0.5f) / float2(size) * 2.0f - 1.0f;
    p.y = -p.y;

    // Triangle vertices (CCW in y-up NDC)
    float2 v0 = float2(0.0f, 0.7f);
    float2 v1 = float2(-0.7f, -0.5f);
    float2 v2 = float2(0.7f, -0.5f);

    // Barycentric coordinates
    float d = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
    float w0 = ((v1.y - v2.y) * (p.x - v2.x) + (v2.x - v1.x) * (p.y - v2.y)) / d;
    float w1 = ((v2.y - v0.y) * (p.x - v2.x) + (v0.x - v2.x) * (p.y - v2.y)) / d;
    float w2 = 1.0f - w0 - w1;

    float4 color = float4(0.05f, 0.05f, 0.08f, 1.0f);
    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
        float4 c0 = float4(1.0f, 0.2f, 0.2f, 1.0f);
        float4 c1 = float4(0.2f, 1.0f, 0.2f, 1.0f);
        float4 c2 = float4(0.2f, 0.2f, 1.0f, 1.0f);
        color = w0 * c0 + w1 * c1 + w2 * c2;
    }
    img.write(coord, color);
    return 0;
}
