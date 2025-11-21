#pragma once
#include <luisa/std.hpp>
#include <luisa/printer.hpp>
#include <surfel/hash_table.hpp>
using namespace luisa::shader;
#define SURFEL_INT_SIZE 9
#define OFFLINE_SURFEL_INT_SIZE 4
#define MAX_ACCUM 4096
// struct Surfel {
// 	std::array<float, 3> radiance;
// 	uint accum_count;
// 	uint frame_index;
// 	std::array<float, 3> pos;
// 	uint confidence;
// };

// struct IndirectSurfel {
// 	std::array<float, 3> radiance;
// 	uint frame_index;
// 	std::array<float, 3> pos;
// 	uint normal;
// };

static bool get_grid_level(
	float3 cam_pos,
	float grid_size,
	float3 world_pos,
	int& level) {
	int3 grid_coord;
	auto grid_coord_flt = world_pos * grid_size;
	grid_coord = ite(grid_coord_flt < float3(0.f), int3(grid_coord_flt) - 1, int3(grid_coord_flt));
	auto cam_grid_coord_flt = cam_pos * grid_size;
	auto cam_grid_coord = ite(cam_grid_coord_flt < float3(0.f), int3(cam_grid_coord_flt) - 1, int3(cam_grid_coord_flt));
	grid_coord -= cam_grid_coord;
	int3 log_level = int3(max(float3(5.1f), log2(abs(float3(ite(grid_coord < int3(0), grid_coord + 1, grid_coord))) + 0.1f))) - 5;
	level = max(log_level.x, max(log_level.y, log_level.z));
	return level < 255;
}

static bool temporal_get_grid_key(
	uint buffer_size,
	uint max_offset,
	float3 world_pos,
	uint normal_face,
	float3 cam_pos,
	float grid_size,
	uint& key,
	int& level) {
	int3 grid_coord;
	auto grid_coord_flt = world_pos * grid_size;
	grid_coord = ite(grid_coord_flt < float3(0.f), int3(grid_coord_flt) - 1, int3(grid_coord_flt));
	auto cam_grid_coord_flt = cam_pos * grid_size;
	auto cam_grid_coord = ite(cam_grid_coord_flt < float3(0.f), int3(cam_grid_coord_flt) - 1, int3(cam_grid_coord_flt));
	grid_coord -= cam_grid_coord;
	int3 log_level = int3(max(float3(5.1f), log2(abs(float3(ite(grid_coord < int3(0), grid_coord + 1, grid_coord))) + 0.1f))) - 5;
	level = max(log_level.x, max(log_level.y, log_level.z));
	if (level >= 255) {
		return false;
	}
	// int3 neg_grid_coord = grid_coord + 1;
	// neg_grid_coord >>= level;
	// neg_grid_coord -= 1;
	grid_coord += int3(64) * int(exp2(float(level)) + 0.5f);
	grid_coord = grid_coord >> level;

	// key <<= 7u;
	key = grid_coord.x;// 25
	key <<= 7u;
	key |= grid_coord.y;// 18
	key <<= 7u;
	key |= grid_coord.z;//11
	key <<= 3u;
	key |= normal_face;//8
	key <<= 8u;
	key |= level;
	return true;
}

static bool get_grid_key(
	uint buffer_size,
	uint max_offset,
	float3 world_pos,
	float3 normal,
	float3 cam_pos,
	float grid_size,
	uint& key) {
	int level;
	float3 abs_normal = abs(normal);
	uint face = 0;
	if (abs_normal.y > abs_normal.z && abs_normal.y > abs_normal.x) {
		face = ite(normal.y < 0, 4, 1);
	} else if (abs_normal.z > abs_normal.y && abs_normal.z > abs_normal.x) {
		face = ite(normal.z < 0, 5, 2);
	} else {
		face = ite(normal.x < 0, 3, 0);
	}

	return temporal_get_grid_key(buffer_size, max_offset, world_pos, face, cam_pos, grid_size, key, level);
}