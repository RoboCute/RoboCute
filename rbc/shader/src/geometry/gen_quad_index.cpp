#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
	Buffer<uint>& buffer) {
    auto size = dispatch_size().xy;
    auto id =  dispatch_id().xy;
    auto write_idx = (size.x * id.y + id.x) * 6;
    auto v0_idx = id.y * (size.x + 1) + id.x;
    auto v1_idx = id.y * (size.x + 1) + id.x + 1;
    auto v2_idx = (id.y + 1) * (size.x + 1) + id.x;
    auto v3_idx = (id.y + 1) * (size.x + 1) + id.x + 1;
    buffer.write(write_idx, v0_idx);
    buffer.write(write_idx + 1, v1_idx);
    buffer.write(write_idx + 2, v2_idx);
    buffer.write(write_idx + 3, v1_idx);
    buffer.write(write_idx + 4, v3_idx);
    buffer.write(write_idx + 5, v2_idx);
	return 0;
}