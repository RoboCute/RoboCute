#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
#endif

struct TraceSurfelArgs {
	float sky_confidence;
	// hdri
	float3x3 world_2_sky_mat;
	uint sky_heap_idx;
	uint alias_table_idx;
	uint pdf_table_idx;
	uint frame_index;
	// light_bvh
	uint light_count;
	// surfel
	float grid_size;
	uint buffer_size;
	uint max_offset;
};