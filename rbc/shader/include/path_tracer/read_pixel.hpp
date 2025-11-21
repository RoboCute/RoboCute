#pragma once
using namespace luisa::shader;
struct PixelBufferData {
	uint2 coord;
	uint spp;
};
inline PixelBufferData read_pixels(uint pixel_buffer_data){
    PixelBufferData d;
    d.spp = (pixel_buffer_data >> uint(28)) + 1;
    d.coord = uint2((pixel_buffer_data >> 14u) & 16383u, pixel_buffer_data & 16383u);
    return d;
}
// packed_coord = pixel_buffer.read(dispatch_id().x)
