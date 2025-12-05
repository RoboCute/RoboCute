#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<uint>& img,
    uint4 value
) {
    img.write(dispatch_id().xy, value);
    return 0;
}