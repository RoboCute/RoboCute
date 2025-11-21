#if (defined(OFFLINE_MODE) && !defined(PT_ARGS_OFFLINE_MODE_INCLUDE)) || (!defined(OFFLINE_MODE) && !defined(PT_ARGS_REALTIME_MODE_INCLUDE))

#ifndef __SHADER_LANG__
#ifdef OFFLINE_MODE
#define PT_ARGS_OFFLINE_MODE_INCLUDE
#else
#define PT_ARGS_REALTIME_MODE_INCLUDE
#endif
#endif

#define NO_RESTIR_DI
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
#endif

#ifdef OFFLINE_MODE
inline namespace offline {
#else
inline namespace realtime {
#endif
struct PTArgs {
#ifndef OFFLINE_MODE
	float sky_confidence;
	float default_depth;
#endif
	float3x3 resource_to_rec2020_mat;
	// hdri
	float3x3 world_2_sky_mat;
	uint sky_heap_idx;
	uint sky_lum_idx;
	uint alias_table_idx;
	uint pdf_table_idx;
	// cam
	float3 cam_pos;
	float2 jitter_offset;
	float4x4 inv_view;
	float4x4 view;
	float4x4 inv_vp;
#ifndef OFFLINE_MODE
	float4x4 vp;
	float4x4 last_vp;
#endif
	uint frame_index;
	uint frame_countdown;
	uint light_count;
	float2 tex_grad_scale;// render_size / display_size
	bool enable_physical_camera;
	float focus_distance;
	float lens_radius;
	float time;
#ifdef OFFLINE_MODE
	uint bounce;
	float gbuffer_temporal_weight;
	bool reset_emission;
#else
	float grid_size;
	uint buffer_size;
	uint max_offset;
#endif
};
struct MultiBouncePixel {
	std::array<float, 3> beta;
	uint pixel_id;
	std::array<float, 3> input_pos;
	float pdf_bsdf;
	std::array<float, 3> input_dir;
    float length_sum;
};
}
#endif