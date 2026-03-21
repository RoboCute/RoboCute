#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace geometry {
using namespace luisa::shader;
}// namespace geometry
#else
#include <luisa/core/basic_types.h>
namespace geometry {
using float3 = luisa::float3;
using float2 = luisa::float2;
using float4 = luisa::float4;
using float4x4 = luisa::float4x4;
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