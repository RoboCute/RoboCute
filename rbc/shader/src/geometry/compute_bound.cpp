#define DEBUG
#include <luisa/printer.hpp>
#include <luisa/std.hpp>
#include <luisa/functions/atomic.hpp>
using namespace luisa::shader;
[[kernel_1d(256)]] int kernel(
	BindlessBuffer& buffer_heap,
	Buffer<uint2>& offset_buffer,// x: vertex_heap_idx  y: tri_element_offset
	Buffer<uint>& result_aabb,
	bool clear) {
	auto id = dispatch_id().x;
	if (clear) {
		bool bound = (id % 6) < 3;
		result_aabb.write(id, bound ? float_pack_to_uint(1e20f) : float_pack_to_uint(-1e20f));
		return 0;
	}
	auto thd_id = thread_id().x;
	auto offset = offset_buffer.read(kernel_id());
	auto vert_id = buffer_heap.buffer_read<uint>(offset.x, offset.y + id);
	auto vert = buffer_heap.buffer_read<float3>(offset.x, vert_id);
	for (int i = 0; i < 3; ++i) {
		result_aabb.atomic_fetch_min(kernel_id() * 6 + i, float_pack_to_uint(vert[i]));
		result_aabb.atomic_fetch_max(kernel_id() * 6 + i + 3, float_pack_to_uint(vert[i]));
	}
	return 0;
}