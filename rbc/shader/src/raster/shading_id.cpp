#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<uint> &img,
    Image<float> &dst_img) {
    auto id = dispatch_id().xy;
    auto src = img.read(id);
    float3 col;
    float2 bary(bit_cast<float>(src.z), bit_cast<float>(src.w));
    if (src.x == max_uint32) {
        col = float3(0.3f, 0.6f, 0.7f);
    } else if (src.x == 0) {
        col = float3(1, bary);
    } else if (src.x == 1) {
        col = float3(bary.x, 1, bary.y);
    } else if (src.x == 2) {
        col = float3(bary, 1);
    } else {
        col = float3(bary, 0);
    }
    dst_img.write(id, float4(col, 1.f));
    return 0;
}