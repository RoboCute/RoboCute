#pragma once
// #define OUTPUT_DENOISE_TRAINING_DATA
#ifdef OUTPUT_DENOISE_TRAINING_DATA
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
namespace ai {
using PackedVector2 = std::array<float, 2>;
using PackedVector3 = std::array<float, 3>;
#else
using PackedVector2 = std::array<float, 2>;
using PackedVector3 = std::array<float, 3>;
#endif
struct DenoiseTrainingData {
	PackedVector3 di_color;
	PackedVector3 gi_color;
	PackedVector3 normal;
	PackedVector3 albedo;
	PackedVector3 gi_indirect_albedo;
	PackedVector3 gi_indirect_hit_dir;
	PackedVector2 motion_vec;// [0, 1] uv space motion_vectors
	float gi_indirect_hit_dist;
	float view_dist;
	float roughness;
};
#ifdef __SHADER_LANG__
inline std::array<float, 3> pack_float3(float3 v) {
	std::array<float, 3> r;
	r[0] = v.x;
	r[1] = v.y;
	r[2] = v.z;
	return r;
}
inline std::array<float, 2> pack_float2(float2 v) {
	std::array<float, 2> r;
	r[0] = v.x;
	r[1] = v.y;
	return r;
}
}// namespace ai
#endif
#endif