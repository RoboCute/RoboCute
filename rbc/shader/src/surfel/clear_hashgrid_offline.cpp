#include <surfel/hash_table.hpp>
using namespace luisa::shader;
#include <surfel/surfel_grid.hpp>

[[kernel_1d(128)]] int kernel(
	Buffer<uint>& key_buffer,
	Buffer<uint>& value_buffer,
	uint max_accum) {
	auto id = dispatch_id().x;
	auto value_id = id * OFFLINE_SURFEL_INT_SIZE;
	if (max_accum > 0) {
		auto ori_value = key_buffer.read(id);
		if (ori_value == HASH_EMPTY) return 0;
		value_buffer.write(value_id + 3, min(max_accum, value_buffer.read(value_id + 3)));
	} else {
		key_buffer.write(id, HASH_EMPTY);
		for (int i = 0; i < 4; ++i) {
			value_buffer.write(value_id + i, 0);
		}
	}
	return 0;
}