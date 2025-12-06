#include <luisa/std.hpp>
using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
    Image<float> &img,
    Image<float> &dst_img
) {
    int2 id = dispatch_id().xy;
    auto v = dst_img.read(id) - img.read(id);
    dst_img.write(id, v);
}