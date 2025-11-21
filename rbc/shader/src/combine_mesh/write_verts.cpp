#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <geometry/vertices.hpp>
#include "data.hpp"
using namespace luisa::shader;
[[kernel_1d(128)]] int kernel(
	BindlessBuffer& heap,
	Buffer<Data>& data_buffer,
	uint sum_vert_count,
	bool contained_normal,
	bool contained_tangent,
	uint contained_uv,
	Buffer<float>& dst) {
	auto data = data_buffer.read(kernel_id());
	auto id = dispatch_id().x;
	auto dst_id = data.vert_offset + id;
	// write pos
	float3 pos = float3(heap.uniform_idx_buffer_read<float>(data.heap_idx, id * 4), heap.uniform_idx_buffer_read<float>(data.heap_idx, id * 4 + 1), heap.uniform_idx_buffer_read<float>(data.heap_idx, id * 4 + 2));
	pos = (data.local_to_world * float4(pos, 1.f)).xyz;
	dst.write(dst_id * 4, pos.x);
	dst.write(dst_id * 4 + 1, pos.y);
	dst.write(dst_id * 4 + 2, pos.z);

	uint dst_offset = 4 * sum_vert_count;
	uint self_offset = 4 * data.vertex_count;
	// write normal
	if (contained_normal) {
		float3 normal;
		if (data.contained_normal) {
			normal = float3(
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4),
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4 + 1),
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4 + 2));
			self_offset += 4 * data.vertex_count;
		}
		normal = (data.local_to_world * float4(normal, 0.f)).xyz;
		dst.write(dst_offset + dst_id * 4, normal.x);
		dst.write(dst_offset + dst_id * 4 + 1, normal.y);
		dst.write(dst_offset + dst_id * 4 + 2, normal.z);
		dst_offset += 4 * sum_vert_count;
	}
	if (contained_tangent) {
		float4 tangent;
		if (data.contained_tangent) {
			tangent = float4(
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4),
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4 + 1),
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4 + 2),
				heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 4 + 3));
			self_offset += 4 * data.vertex_count;
		}
		tangent = float4((data.local_to_world * float4(tangent.xyz, 0.f)).xyz, tangent.w);
		dst.write(dst_offset + dst_id * 4, tangent.x);
		dst.write(dst_offset + dst_id * 4 + 1, tangent.y);
		dst.write(dst_offset + dst_id * 4 + 2, tangent.z);
		dst.write(dst_offset + dst_id * 4 + 3, tangent.w);
		dst_offset += 4 * sum_vert_count;
	}
	for (uint uv_idx = 0; uv_idx < min(contained_uv, data.contained_uv); ++uv_idx) {
		float2 uv = float2(
			heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 2),
			heap.uniform_idx_buffer_read<float>(data.heap_idx, self_offset + id * 2 + 1));
		self_offset += 2 * data.vertex_count;
		dst.write(dst_offset + dst_id * 2, uv.x);
		dst.write(dst_offset + dst_id * 2 + 1, uv.y);
		dst_offset += 2 * sum_vert_count;
	}
	return 0;
}