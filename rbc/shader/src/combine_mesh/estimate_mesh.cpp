#include <luisa/printer.hpp>
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <geometry/vertices.hpp>
#include <sampling/pcg.hpp>
#include <material/mats.hpp>
#include <post_process/colors.hpp>
using namespace luisa::shader;
[[kernel_1d(128)]] int kernel(
	BindlessBuffer& heap,
	BindlessImage& image_heap,
	geometry::MeshMeta mesh_meta,
	uint mat_index,
	uint mat_idx_buffer_heap_idx,
	Buffer<float>& result) {
	auto prim_id = dispatch_id().x;
	sampling::PCGSampler pcg(prim_id);
	float3 sum;
	constexpr uint SPP = 16;
	auto pos_uv = geometry::read_vert_pos_uv(heap, prim_id, mesh_meta);
	float a = distance(pos_uv[0].pos, pos_uv[1].pos);
	float b = distance(pos_uv[0].pos, pos_uv[2].pos);
	float c = distance(pos_uv[1].pos, pos_uv[2].pos);
	float p = (a + b + c) / 2.0f;
	float area = sqrt(p * (p - a) * (p - b) * (p - c)) * 2.f;
	for (uint i = 0; i < SPP; ++i) {
		auto bary = pcg.next2f();
		auto uv = geometry::interpolate(bary, pos_uv[0].uv, pos_uv[1].uv, pos_uv[2].uv);
		auto emission = material::get_light_emission(heap, image_heap, mat_idx_buffer_heap_idx, mesh_meta.submesh_heap_idx, mat_index, prim_id, uv);
		sum += emission;
	}
	sum = sum / float(SPP);
	auto lum = Luminance(sum) * area;
	result.write(prim_id, lum);
	return 0;
}