#include "data.hpp"
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <geometry/vertices.hpp>
using namespace luisa::shader;
[[kernel_1d(128)]] int kernel(
	Buffer<uint>& dst,
	BindlessBuffer& heap,
	Buffer<Data>& data_buffer) {
	auto id = dispatch_id().x;
	auto data = data_buffer.read(kernel_id());
	auto tri_idx = heap.uniform_idx_buffer_read<uint>(data.heap_idx, data.tri_elem_offset + id);
	tri_idx += data.vert_offset;
	dst.write(data.index_offset + id, tri_idx);
    return 0;
}