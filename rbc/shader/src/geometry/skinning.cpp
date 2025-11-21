#include <luisa/std.hpp>
#include <geometry/dual_quaternion.hpp>
#include <geometry/vertices.hpp>

using namespace luisa::shader;

[[kernel_2d(128, 1)]] int kernel(
	ByteBuffer<>& src_buffer,
	ByteBuffer<>& dst_buffer,
	Buffer<float3>& last_pos_buffer,
	Buffer<geometry::DualQuaternion>& dq_bone_buffer,// bone matrix in raw (4 colume, 3 raw)
	Buffer<uint>& bone_indices,
	Buffer<float>& bone_weights,
	uint bones_count_per_vert,
	uint vertex_count,
	bool contained_normal,
	bool contained_tangent,
	bool reset_frame) {
	auto vert_id = dispatch_id().x;
	auto pos_normal = geometry::read_pos_normal(src_buffer, vert_id, vertex_count, contained_normal, contained_tangent);
	auto buffer_idx = vert_id * bones_count_per_vert;
	geometry::DualQuaternion dq0;
	geometry::DualQuaternion blend_dq;
	uint index;
	float weight;
	for (uint i = 0; i < bones_count_per_vert; ++i) {
		index = bone_indices.read(buffer_idx + i);
		weight = bone_weights.read(buffer_idx + i);
		geometry::DualQuaternion dq = dq_bone_buffer.read(index);
		if (i > 0) {
			dq = DualQuaternionShortestPath(dq, dq0);
		} else {
			dq0 = dq;
		}
		blend_dq.rotation_quaternion += dq.rotation_quaternion * weight;
		blend_dq.translation_quaternion += dq.translation_quaternion * weight;
	}
	float mag = length(blend_dq.rotation_quaternion);
	blend_dq.rotation_quaternion /= mag;
	blend_dq.translation_quaternion /= mag;
	pos_normal.pos = geometry::QuaternionApplyRotation(float4(pos_normal.pos, 1.f), blend_dq.rotation_quaternion).xyz;
	pos_normal.normal = geometry::QuaternionApplyRotation(float4(pos_normal.normal, 0.f), blend_dq.rotation_quaternion).xyz;
	pos_normal.tangent = geometry::QuaternionApplyRotation(pos_normal.tangent, blend_dq.rotation_quaternion);
	pos_normal.pos += geometry::QuaternionMultiply(blend_dq.translation_quaternion, geometry::QuaternionInvert(blend_dq.rotation_quaternion)).xyz;

	float3 last_pos;
	if (reset_frame) {
		last_pos = pos_normal.pos;
	} else {
		last_pos = dst_buffer.byte_read<float3>(16 * vert_id);
	}
	last_pos_buffer.write(vert_id, last_pos);
	geometry::write_pos_normal(
		dst_buffer,
		vert_id,
		vertex_count,
		contained_normal,
		contained_tangent,
		pos_normal);

	return 0;
}

// float4x4 dq_to_matrix(float4 qn, float4 qd) {
// 	float4x4 M(1.f, 0.f, 0.f, 0.f,
// 			   0.f, 1.f, 0.f, 0.f,
// 			   0.f, 0.f, 1.f, 0.f,
// 			   0.f, 0.f, 0.f, 1.f);
// 	float len2 = dot(qn, qn);
// 	float w = qn.x, x = qn.y, y = qn.z, z = qn.w;
// 	float t0 = qd.x, t1 = qd.y, t2 = qd.z, t3 = qd.w;
// 	M[0][0] = w * w + x * x - y * y - z * z;
// 	M[0][1] = 2 * x * y - 2 * w * z;
// 	M[0][2] = 2 * x * z + 2 * w * y;
// 	M[1][0] = 2 * x * y + 2 * w * z;
// 	M[1][1] = w * w + y * y - x * x - z * z;
// 	M[1][2] = 2 * y * z - 2 * w * x;
// 	M[2][0] = 2 * x * z - 2 * w * y;
// 	M[2][1] = 2 * y * z + 2 * w * x;
// 	M[2][2] = w * w + z * z - x * x - y * y;

// 	M[0][3] = -2 * t0 * x + 2 * w * t1 - 2 * t2 * z + 2 * y * t3;
// 	M[1][3] = -2 * t0 * y + 2 * t1 * z - 2 * x * t3 + 2 * w * t2;
// 	M[2][3] = -2 * t0 * z + 2 * x * t2 + 2 * w * t3 - 2 * t1 * y;
// 	return transpose(M);//TODO: need transpose ?
// }