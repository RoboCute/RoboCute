#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace geometry {
using namespace luisa::shader;
}// namespace geometry
#endif

namespace geometry {
struct MeshMeta {
	uint heap_idx;
	uint mutable_heap_idx;
	uint tri_byte_offset;
	uint vertex_count;
	uint ele_mask;
	uint submesh_heap_idx;
	static constexpr uint normal_mask = 1 << 0;
	static constexpr uint tangent_mask = 1 << 1;
	static constexpr uint uv_mask = 1 << 2;
};
struct InstanceInfo {
	MeshMeta mesh;
	uint mat_index;
	uint light_mask_id;
	uint last_vertex_heap_idx;
	uint get_light_mask() {
		return light_mask_id >> 24u;
	}
	uint get_light_id() {
		return light_mask_id & (0xffffffu);
	}
};
using Triangle = std::array<uint, 3>;
struct Vertex {
	float3 pos;
	float3 normal;
	float4 tangent;
	std::array<float2, 4> uvs;
};
struct RasterElement {
	float4x4 local_to_world_and_inst_id;// inst_id = reinterpret_cast<uint&>(local_to_world[3][3])
};
}// namespace geometry