#include <surfel/hash_table.hpp>
using namespace luisa::shader;
#include <surfel/surfel_grid.hpp>

[[kernel_1d(128)]] int kernel(
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	bool reset) {
	auto id = dispatch_id().x;
	if (!reset) {
		auto ori_value = key_buffer.read(id);
		if (ori_value == HASH_EMPTY) return 0;
	}
	key_buffer.write(id, HASH_EMPTY);
	auto value_id = id * SURFEL_INT_SIZE;
	for (int i = 0; i < 4; ++i) {
		value_buffer.write(value_id + i, 0);
	}
	value_buffer.write(value_id + 4, max_uint32);
	value_buffer.write(value_id + 8, max_uint32);
	return 0;
}