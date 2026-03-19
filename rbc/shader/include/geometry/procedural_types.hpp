#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace geometry {
using namespace luisa::shader;
}// namespace geometry
#endif
namespace geometry {
	
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