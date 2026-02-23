#ifndef TYPE
#define TYPE float
#endif

#ifndef TEXTURE_TYPE
#define TEXTURE_TYPE TYPE
#endif

#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
    Image<TEXTURE_TYPE> &input_img,
    Buffer<TYPE> &output_buffer,
    uint swizzle_bytes,// 4 channel position, 255 is invalid
    uint2 pixel_offset // input coord offset
) {
    // Get dispatch id (pixel position in output)
    auto id = dispatch_id().xy;
    auto size = dispatch_size().xy;

    // Calculate input pixel coordinate with offset
    auto input_coord = id + pixel_offset;

    // Read from input image
    auto color = input_img.read(input_coord);

    // Calculate output buffer index (linear index from 2D position)
    uint channel_count = 0;
    for (uint i = 0; i < 4; ++i) {
        uint swizzle_byte = (swizzle_bytes >> (i * 8)) & 0xFF;
        if (swizzle_byte < 4) {
            channel_count += 1;
        }
    }
    uint buffer_index = (id.x + id.y * size.x) * channel_count;

    // Extract swizzle bytes (4 bytes, each indicating which source channel to use)
    // swizzle_bytes format: byte0 = output channel 0 source, byte1 = output channel 1 source, etc.
    // 255 means invalid/don't write
    channel_count = 0;
    for (uint i = 0; i < 4; ++i) {
        uint swizzle_byte = (swizzle_bytes >> (i * 8)) & 0xFF;
        if (swizzle_byte < 4) {
            // Write the swizzled channel value to buffer
            output_buffer.write(buffer_index + channel_count, (TYPE)color[swizzle_byte]);
            channel_count++;
        }
    }
    return 0;
}
