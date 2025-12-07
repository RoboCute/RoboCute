#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &img,
    float4 color,
    int2 pixel_offset) {
    int2 id = dispatch_id().xy;
    int2 size = dispatch_size().xy;
    if (any(id == size - 1 || id == 0)) {
        color.w = 1.f;
    }
    auto src = img.read(id + pixel_offset);
    img.write(id + pixel_offset, float4(lerp(src.xyz, color.xyz, color.w), src.w));
    return 0;
}