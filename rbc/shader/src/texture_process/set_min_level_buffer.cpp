#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(64)]] int kernel(
    Buffer<uint16> &buffer,
    Buffer<uint2> &offset_buffer,
    Buffer<uint> &chunk_offset_buffer,
    uint value) {
    auto id = dispatch_id().x;
    auto offset = offset_buffer.read(kernel_id());
    if (chunk_offset_buffer.read(offset.y) != max_uint32) return 0;
    buffer.write(offset.x + id, value);
    return 0;
}