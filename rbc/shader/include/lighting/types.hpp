#pragma once
#include <luisa/std.hpp>
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
trait_struct LightTypes {
	static constexpr uint32 PointLight = 0;
	static constexpr uint32 SpotLight = 1;
	static constexpr uint32 AreaLight = 2;
	static constexpr uint32 MeshLight = 3;
	static constexpr uint32 DiskLight = 4;
	static constexpr uint32 LightCount = 5;
	static constexpr uint32 Blas = 6;
};
}// namespace lighting
