#include <luisa/std.hpp>
#include <geometry/vertices.hpp>
#include <material/mat_codes.hpp>
using namespace luisa::shader;
struct RayInput {
	std::array<float, 3> ray_origin;
	std::array<float, 3> ray_dir;
	float t_min;
	float t_max;
	uint mask;
};

struct RayOutput {
	uint mat_code;
	float ray_t;
	uint tlas_inst_id;
	uint blas_prim_id;
	uint blas_submesh_id;
	std::array<float, 2> triangle_bary;
};

[[kernel_1d(128)]] int kernel(
	Accel& accel,
	BindlessBuffer& heap,
	Buffer<RayInput>& ray_input_buffer,
	Buffer<RayOutput>& ray_output_buffer,
	uint inst_buffer_heap_idx,
	uint mat_idx_buffer_heap_idx) {
	auto id = dispatch_id().x;
	auto input = ray_input_buffer.read(id);
	Ray ray(
		float3(input.ray_origin[0], input.ray_origin[1], input.ray_origin[2]),
		float3(input.ray_dir[0], input.ray_dir[1], input.ray_dir[2]),
		input.t_min,
		input.t_max);
	auto hit = accel.trace_closest(ray, input.mask);
	RayOutput output;
	if (hit.miss()) {
		output.mat_code = max_uint32;
		output.ray_t = -1.f;
		output.tlas_inst_id = max_uint32;
		output.blas_prim_id = max_uint32;
		output.blas_submesh_id = max_uint32;
		ray_output_buffer.write(id, output);
		return 0;
	}
	auto user_id = accel.instance_user_id(hit.inst);
	auto inst_info = heap.uniform_idx_buffer_read<geometry::InstanceInfo>(inst_buffer_heap_idx, user_id);
	output.tlas_inst_id = user_id;
	output.blas_prim_id = hit.prim;
	output.blas_submesh_id = geometry::get_submesh_idx(heap, inst_info.mesh.submesh_heap_idx, hit.prim);
	output.ray_t = hit.ray_t;
	output.mat_code = material::_mat_code(heap, mat_idx_buffer_heap_idx, inst_info.mesh.submesh_heap_idx, inst_info.mat_index, hit.prim);
	output.triangle_bary[0] = hit.bary.x;
	output.triangle_bary[1] = hit.bary.y;
	ray_output_buffer.write(id, output);
	return 0;
}