#pragma once
#include <luisa/std.hpp>
namespace mtl {
using namespace luisa::shader;
struct Onb {
	float3 tangent{1.0f, 0.0f, 0.0f};
	float3 bitangent{0.0f, 1.0f, 0.0f};
	float3 normal{0.0f, 0.0f, 1.0f};
	Onb() = default;
	Onb(float3 normal) : normal{normal} {
		// https://jcgt.org/published/0006/01/01/
		float sign = copysign(1.0f, normal.z);
		const float a = -rcp(sign + normal.z);
		const float b = normal.x * normal.y * a;
		tangent = float3(1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x);
		bitangent = float3(b, sign + normal.y * normal.y * a, -normal.y);
	}
	void rotate_tangent(float2 cs) {
		float3 new_tangent = tangent * cs.x - bitangent * cs.y;
		bitangent = tangent * cs.y + bitangent * cs.x;
		tangent = new_tangent;
	}
	void rotate_tangent(float angle) {
		rotate_tangent(float2{cos(angle), sin(angle)});
	}
	float3 to_world(float3 v) const {
		return v.x * tangent + v.y * bitangent + v.z * normal;
	}
	float3 to_local(float3 v) const {
		return transpose(float3x3(tangent, bitangent, normal)) * v;
	}
	void replace_normal(float3 new_local_normal, float3 view_dir) {
		auto new_normal = to_world(new_local_normal);
		// HACKING BACKFACE NORMAL
		float curve_start = dot(view_dir, normal);
		float flip = curve_start < 0.0f ? -1.0f : 1.0f;
		curve_start *= flip;
		new_normal *= flip;
		float nov = dot(new_normal, view_dir);
		float3 edge_normal = new_normal - view_dir * nov;
		float weight = saturate(curve_start - curve_start * (curve_start > 0.0f ? exp(nov / curve_start - 1.0f) : 0.0f));
		weight = max(nov, curve_start) - weight;
		normal = normalize(view_dir * weight +
						   edge_normal * rsqrt(dot(edge_normal, edge_normal) /
											   max(1.0f - sqr(weight), 1e-10f)));
		normal *= flip;
		bitangent = normalize(cross(normal, tangent));
		tangent = normalize(cross(normal, bitangent));
	}
};
}// namespace mtl