#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace geometry {
using namespace luisa::shader;
}// namespace geometry
#endif
namespace geometry {

struct HeightMap {
	float2 uv_scale;
	float2 uv_offset;
	uint heightmap_idx;
	uint filter_address;
	uint filter() {
		return filter_address & 3;
	}
	uint address() {
		return (filter_address >> 2) & 3;
	}
	float2 uv(float2 local_pos_xz) {
		return local_pos_xz * uv_scale + uv_offset;
	}
};
struct VoxelSurface {
	uint aabb_buffer_heap_idx;
	uint aabb_buffer_offset;// AABB element idx
};

struct SDFMap {
	uint volume_idx;
	uint sample_count;
	float3 uvw_scale;
	float3 uvw_offset;
};
struct ProceduralType {
	uint type;
	uint meta_byte_offset;
	// TODO: material indices
};
}// namespace geometry