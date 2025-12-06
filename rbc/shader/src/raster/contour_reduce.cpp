#include <luisa/std.hpp>
using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
    Image<float> &img,
    Image<float> &dst_img,
    Image<float> &result,
    float3 col) {
    int2 id = dispatch_id().xy;
    auto v = abs(dst_img.read(id).x - img.read(id).x);
    col *= v;
    auto src = result.read(id);
    result.write(id, float4(col + src.xyz, src.w));
    return 0;
}