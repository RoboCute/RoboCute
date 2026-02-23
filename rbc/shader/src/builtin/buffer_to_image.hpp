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
    Buffer<TYPE> &input_buffer,
    Image<TEXTURE_TYPE> &output_img,
    uint swizzle_bytes,// 4 channel position, 255 is invalid
    uint2 pixel_offset // output coord offset
) {
    // Get dispatch id (pixel position in output)
    auto id = dispatch_id().xy;
    auto size = dispatch_size().xy;

    // Calculate output pixel coordinate with offset
    auto output_coord = id + pixel_offset;

    // Calculate input buffer index (linear index from 2D position)
    uint channel_count = 0;
    for (uint i = 0; i < 4; ++i) {
        uint swizzle_byte = (swizzle_bytes >> (i * 8)) & 0xFF;
        if (swizzle_byte < 4) {
            channel_count += 1;
        }
    }
    uint buffer_index = (id.x + id.y * size.x) * channel_count;

    // Read from buffer and assemble output color
    // swizzle_bytes format: byte0 = output channel 0 source, byte1 = output channel 1 source, etc.
    // 255 means invalid/don't read (use 0)
    vec<TEXTURE_TYPE, 4> color = vec<TEXTURE_TYPE, 4>(0, 0, 0, 0);
    channel_count = 0;
    for (uint i = 0; i < 4; ++i) {
        uint swizzle_byte = (swizzle_bytes >> (i * 8)) & 0xFF;
        if (swizzle_byte < 4) {
            // Read from buffer and assign to the corresponding output channel
            color[i] = (TEXTURE_TYPE)input_buffer.read(buffer_index + channel_count);
            channel_count++;
        }
    }

    // Write to output image
    output_img.write(output_coord, color);
    return 0;
}
