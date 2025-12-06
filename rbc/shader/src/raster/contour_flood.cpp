#include <luisa/std.hpp>
using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
    Image<float> &img,
    Image<float> &dst_img,
    int2 direction,
    int step,
    float atten_pow) {
    int2 id = dispatch_id().xy;
    int2 size = dispatch_size().xy;
    float value = img.read(id).x;
    for (int i = -step; i <= step; ++i) {
        if (i == 0) continue;
        auto col = img.read(clamp(id + direction * i, int2(0), int2(size - 1))).x;
        col *= 1.0f - pow(saturate((abs((float)i) - 0.5f) / (float)step), atten_pow);
        value = max(value, col);
    }
    dst_img.write(id, value);
    return 0;
}