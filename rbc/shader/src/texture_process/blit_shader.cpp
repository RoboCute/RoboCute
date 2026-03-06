#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    SampleImage &src_img,
    Image<float> &dst_img,
    float2 src_uv_scale,
    float2 src_uv_offset,
    uint2 dst_offset_pixel) {
    auto id = dispatch_id().xy;
    auto size = dispatch_size().xy;
    auto uv = (float2(id) + 0.5f) / float2(size);
    uv = uv * src_uv_scale + src_uv_offset;
    auto color = src_img.sample(uv, Filter::LINEAR_POINT, Address::EDGE);
    color.w = 0.5f;
    dst_img.write(id + dst_offset_pixel, color);
    return 0;
}
