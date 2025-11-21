#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;

float3 get_pos(ByteBuffer<>& vert_buffer, uint2 id) {
	return vert_buffer.byte_read<float3>(16u * (id.x + id.y * dispatch_size().x));
}

float3 get_normal(
	ByteBuffer<>& vert_buffer,
	uint2 id0,
	uint2 id1,
	uint2 id2) {
	float3 v0 = get_pos(vert_buffer, id0);
	float3 v1 = get_pos(vert_buffer, id1);
	float3 v2 = get_pos(vert_buffer, id2);
	auto normal = normalize(cross(v0 - v1, v0 - v2));
	if (normal.y < 0) {
		normal = -normal;
	}
	return normal;
}

[[kernel_2d(16, 8)]] int kernel(
	ByteBuffer<>& vert_buffer,
	uint vertex_count,
	bool write_uv,
	float2 uv_scale,
	float2 uv_offset) {
	auto id = dispatch_id().xy;
	auto size = dispatch_size().xy;
	auto vert_id = id.x + id.y * size.x;
	float4 normal(0);
	if (id.x > 0) {
		if (id.y > 0) {
			normal += float4(get_normal(vert_buffer, uint2(id.x - 1, id.y - 1), uint2(id.x - 1, id.y), id), 1);
			normal += float4(get_normal(vert_buffer, uint2(id.x - 1, id.y - 1), uint2(id.x, id.y - 1), id), 1);
		}
		if (id.y < size.y - 1) {
			normal += float4(get_normal(vert_buffer, uint2(id.x - 1, id.y + 1), uint2(id.x - 1, id.y), id), 1);
			normal += float4(get_normal(vert_buffer, uint2(id.x - 1, id.y + 1), uint2(id.x, id.y + 1), id), 1);
		}
	}
	if (id.x < size.x - 1) {
		if (id.y > 0) {
			normal += float4(get_normal(vert_buffer, uint2(id.x + 1, id.y - 1), uint2(id.x + 1, id.y), id), 1);
			normal += float4(get_normal(vert_buffer, uint2(id.x + 1, id.y - 1), uint2(id.x, id.y - 1), id), 1);
		}
		if (id.y < size.y - 1) {
			normal += float4(get_normal(vert_buffer, uint2(id.x + 1, id.y + 1), uint2(id.x + 1, id.y), id), 1);
			normal += float4(get_normal(vert_buffer, uint2(id.x + 1, id.y + 1), uint2(id.x, id.y + 1), id), 1);
		}
	}
	if (normal.w > 1e-5f) {
		normal = float4(normalize(normal.xyz), 1.f);
	} else {
		normal = float4(0, 1, 0, 1);
	}
	float3 hori_vec = float3(1, 0, 0);
	if (abs(dot(hori_vec, normal.xyz)) > 0.99) {
		hori_vec = float3(0, 0, 1);
	}
	float4 tangent = float4(normalize(cross(normal.xyz, hori_vec.xyz)), 1);
	vert_buffer.byte_write(16 * vertex_count + 16 * vert_id, normal.xyz);
	vert_buffer.byte_write(32 * vertex_count + 16 * vert_id, tangent);
	if (write_uv) {
		auto uv = float2(dispatch_id().xy) / float2(size - 1u);
		uv = uv * uv_scale + uv_offset;
		vert_buffer.byte_write(48 * vertex_count + 8 * vert_id, uv);
	}
	return 0;
}