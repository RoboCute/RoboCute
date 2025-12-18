#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(Buffer<BUFFER_TYPE> &src_buffer, Buffer<BUFFER_TYPE> &dst_buffer, uint2 pack_tile) {
    uint2 coord = dispatch_id().xy;
    uint2 size = dispatch_size().xy;
    uint2 tile_size = size / pack_tile;
    uint2 tile_coord = coord / pack_tile;
    uint tile_idx = tile_coord.x + tile_coord.y * tile_size.x;
    uint pixel_offset = tile_idx * pack_tile.x * pack_tile.y;
    uint2 inner_res = coord % pack_tile;
    uint dst_idx = pixel_offset + inner_res.x + inner_res.y * pack_tile.x;
    dst_buffer.write(dst_idx, src_buffer.read(coord.x + coord.y * size.x));
    return 0;
}