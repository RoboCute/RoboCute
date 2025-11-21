#include <luisa/std.hpp>
using namespace luisa::shader;
#define KERNEL_SIZE 1024
[[kernel_1d(KERNEL_SIZE)]] int kernel(
	Buffer<uint>& buffer,
	Buffer<uint>& out_indices) {
	SharedArray<uint, KERNEL_SIZE * 2> arr;
	SharedArray<uint, 1> bit_one_offset;
	const int thd_id = thread_id().x;
	auto buffer_idx = KERNEL_SIZE * block_id().x + thd_id;
	for (int bit_mask = 0; bit_mask < 64; ++bit_mask) {
		uint buffer_value;
		if (bit_mask >= 32) {
			buffer_value = buffer.read(buffer_idx * 2 + 1);
		} else {
			buffer_value = buffer.read(buffer_idx * 2);
		}
		if (thd_id == 0) {
			bit_one_offset[0] = 0;
		}
		sync_block();
		uint self_value = ((buffer_value >> (bit_mask & 31)) & 1);
		if (self_value == 0)
			bit_one_offset.atomic_fetch_add(0, 1);
		bool is_one_bit = (self_value == 1);
		uint local_idx = 0;
		for (int b = 0; b < 2; ++b) {
			uint prefix_value = 0;
			uint offset = 0;
			int sum_size = KERNEL_SIZE;
			arr[thd_id] = self_value;
			sync_block();
			while (sum_size > 1) {
				if (thd_id < sum_size / 2) {
					arr[thd_id + offset + sum_size] = arr[thd_id * 2 + offset] + arr[thd_id * 2 + 1 + offset];
				}
				sync_block();
				offset += sum_size;
				sum_size /= 2;
			}
			offset -= sum_size * 2;
			sum_size = 2;
			int min_v = 0;
			while (sum_size <= KERNEL_SIZE) {
				auto rate = (KERNEL_SIZE / sum_size);
				auto local_idx = (thd_id - min_v) / rate;
				if (local_idx > 0) {
					auto idx = offset + (thd_id / rate) - 1;
					prefix_value += arr[idx];
					min_v += KERNEL_SIZE / sum_size;
				}
				sum_size *= 2;
				offset -= sum_size;
			}
			if (b == 0) {
				if (is_one_bit) {
					local_idx = prefix_value + bit_one_offset[0];
				}
			} else if (b == 1) {
				if (!is_one_bit) {
					local_idx = prefix_value;
				}
			}
			self_value = 1 - self_value;
		}
		sync_block();
		arr[local_idx] = buffer_idx;
		sync_block();
		buffer_idx = arr[thd_id];
	}
	out_indices.write(KERNEL_SIZE * block_id().x + thd_id, buffer_idx);
	return 0;
}