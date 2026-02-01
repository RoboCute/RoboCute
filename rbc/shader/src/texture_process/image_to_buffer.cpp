#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Buffer<uint> &buffer,
    Image<float> &img,
    uint2 pixel_offset,
    uint channel_map,
    uint channel_idx_scale,
    uint channel_idx_offset) {
    auto coord = dispatch_id().xy;
    auto dispatch_size_val = dispatch_size().xy;
    auto idx = coord.x + coord.y * dispatch_size_val.x;
    idx *= channel_idx_scale;
    auto img_val = bit_cast<uint4>(img.read(coord + pixel_offset));
    for (uint i = 0; i < 4; ++i) {
        uint map = (channel_map >> (8u * i)) & 0xFFu;
        if (map != 0xFFu) {
            buffer.write(idx, img_val[map & 3]);
            idx += channel_idx_offset;
        }
    }
    return 0;
}
