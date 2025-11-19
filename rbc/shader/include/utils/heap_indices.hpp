#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
namespace heap_indices {
using namespace luisa::shader;
}// namespace heap_indices
#endif
namespace heap_indices {
enum buffer {
	reserved_material_indices = 255,
	// light
	light_bvh_heap_idx,
	area_lights_heap_idx,
	point_lights_heap_idx,
	spot_lights_heap_idx,
	mesh_lights_heap_idx,
	disk_lights_heap_idx,
	// instance
	inst_buffer_heap_idx,
	// noise
	sobol_256d_heap_idx,
	sobol_scrambling_heap_idx,
	// buffer allocator
	buffer_allocator_heap_index,
	mat_idx_buffer_heap_idx = buffer_allocator_heap_index,
	// tex streaming
	chunk_offset_buffer_idx,
	min_level_buffer_idx,
	// procedural-primitives
	procedural_type_buffer_idx,
	BINDLESS_BUFFER_RESERVED_NUM
};
enum tex2d {
	illum_d65_idx,
	cie_xyz_cdfinv_idx,
	BINDLESS_TEX2D_RESERVED_NUM
};
enum tex3d {
	spectrum_lut3d_idx,
	transmission_ggx_energy_idx,
	BINDLESS_TEX3D_RESERVED_NUM
};
}// namespace heap_indices