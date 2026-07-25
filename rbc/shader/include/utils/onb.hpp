#pragma once
#include <luisa/std.hpp>
namespace mtl {
using namespace luisa::shader;

float3 bend_to_hemisphere(float3 dir, float3 hemisphere_normal, float strength = 0.1f) {
    float dir_dot_n = dot(dir, hemisphere_normal);
    float3 dir_perp = dir - dir_dot_n * hemisphere_normal;
    float weight = dir_dot_n + sqrt(sqr(dir_dot_n) + sqr(strength));
    weight = saturate(weight / (1.0f + sqrt(1.0f + sqr(strength))));
    float perp_scale = sqrt(
        max(1.0f - sqr(weight), 0.0f) /
        max(dot(dir_perp, dir_perp), 1e-10f));
    return normalize(hemisphere_normal * weight + dir_perp * perp_scale);
}

float3x3 make_normal_transform(float4x4 transform) {
	float3 x = transform[0].xyz;
	float3 y = transform[1].xyz;
	float3 z = transform[2].xyz;
	return float3x3(cross(y, z), cross(z, x), cross(x, y));
}

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
		float handedness = copysign(1.0f, dot(cross(tangent, bitangent), normal));
		auto new_normal = to_world(new_local_normal);
		float3 view_hemisphere = dot(view_dir, normal) < 0.0f ? -view_dir : view_dir;
		new_normal = bend_to_hemisphere(new_normal, view_hemisphere);
		normal = new_normal;
		bitangent = handedness * normalize(cross(normal, tangent));
		tangent = handedness * normalize(cross(bitangent, normal));
	}
};
}// namespace mtl
