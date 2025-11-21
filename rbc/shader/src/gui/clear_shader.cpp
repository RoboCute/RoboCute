#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float>& img,
    float4 color
) {
    img.write(dispatch_id().xy, color);
    return 0;
}