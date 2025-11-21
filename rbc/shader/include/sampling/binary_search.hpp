#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;
template<typename F>
int binary_search(Buffer<F>& buffer, int buffer_size, F value) {
	int high = buffer_size - 1;
	int low = 0;
	int index = 0;
	while (low <= high) {
		auto mid = low + (high - low) / 2;
		auto bf = buffer.read(mid);
		if (bf < value) {
			low = mid + 1;
			index = low;
		} else if (bf > value) {
			high = mid - 1;
			index = mid;
		} else {
			return mid + 1;
		}
	}
	return index;
}