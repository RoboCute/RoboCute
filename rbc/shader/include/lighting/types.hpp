#pragma once
#include <luisa/std.hpp>
#include <std/ex/type_list.hpp>
namespace lighting {
using namespace luisa::shader;
struct Bounding {
	float3 min;
	float3 max;
	Bounding(
		float3 min,
		float3 max) : min(min), max(max) {}
	float3 size() const {
		return max - min;
	}
	bool point_in_bound(float3 point) const {
		return all(point > min && point < max);
	}
};
struct BVHNode {
	std::array<float, 3> min_v;
	uint index;
	std::array<float, 3> max_v;
	float lum;
	float4 cone;
	Bounding bounding() const {
		return Bounding{
			float3(min_v[0], min_v[1], min_v[2]),
			float3(max_v[0], max_v[1], max_v[2])};
	}
};
struct MeshLight {
	float4x4 transform;
	std::array<float, 3> bounding_min;
	uint blas_heap_idx;
	std::array<float, 3> bounding_max;
	uint instance_user_id;
	float lum;
	float mis_weight;
};

struct AreaLight {
	float4x4 transform;
	std::array<float, 3> radiance;
	float area;
	uint emission;
	float mis_weight;
};
// Disk light can be same as disk light
struct DiskLight {
	std::array<float, 3> forward_dir;
	float area;
	std::array<float, 3> radiance;
	std::array<float, 3> position;
	float mis_weight;
};

struct SpotLight {
	std::array<float, 3> radiance;
	float angle_radian;
	float4 sphere;
	std::array<float, 3> forward_dir;
	float small_angle_radian;
	float angle_atten_power;
	float mis_weight;
	float3 pos() {
		return sphere.xyz;
	}
	void set_pos(float3 pos) {
		sphere.xyz = pos;
	}
	float radius() {
		return sphere.w;
	}
	void set_radius(float radius) {
		sphere.w = radius;
	}
};
struct PointLight {
	float4 sphere;
	std::array<float, 3> radiance;
	float mis_weight;
	float3 pos() {
		return sphere.xyz;
	}
	void set_pos(float3 pos) {
		sphere.xyz = pos;
	}
	float radius() {
		return sphere.w;
	}
	void set_radius(float radius) {
		sphere.w = radius;
	}
};
trait_struct LightTypes {
	static constexpr uint32 PointLight = 0;
	static constexpr uint32 SpotLight = 1;
	static constexpr uint32 AreaLight = 2;
	static constexpr uint32 TriangleLight = 3;
	static constexpr uint32 DiskLight = 4;
	static constexpr uint32 LightCount = 5;
	static constexpr uint32 Blas = 6;
};
}// namespace lighting