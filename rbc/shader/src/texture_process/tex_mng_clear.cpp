#include <luisa/std.hpp>

using namespace luisa::shader;

[[kernel_1d(128)]] int kernel(
	Buffer<uint>& buffer,
	uint value,
	uint mask) {
	auto id = dispatch_id().x;
	auto old_value = buffer.read(id);
	auto new_value = (old_value & (~mask)) | (value & mask);
	buffer.write(id, new_value);
	return 0;
}