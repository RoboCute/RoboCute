#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;
template<typename RandomGenerator>
float4 Fog(
	RandomGenerator& sampler,
	SampleVolume& volume,
	float eyeDepth,
	float2 screenuv,
	float near_plane,
	float far_plane,
	float dist_power) {
	float z = (eyeDepth - near_plane) / (far_plane - near_plane);
	float physicsDistance = lerp(near_plane, far_plane, z);
	z = pow(z, 1.0f / dist_power);
	float3 uvw = float3(screenuv.x, screenuv.y, z);
	uvw.xy += (sampler.next2f() * 2.f - 1.f) / float2(volume.size().xy) * 0.7f;
	float4 vol = volume.sample(uvw, Filter::LINEAR_POINT, Address::EDGE);
	return vol;
}