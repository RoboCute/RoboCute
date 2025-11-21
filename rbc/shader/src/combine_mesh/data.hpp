#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;
struct Data {
	float4x4 local_to_world;
	uint vert_offset;
	uint index_offset;
	uint heap_idx;
	uint vertex_count;
	bool contained_normal;
	bool contained_tangent;
	uint contained_uv;
    uint tri_elem_offset;
};