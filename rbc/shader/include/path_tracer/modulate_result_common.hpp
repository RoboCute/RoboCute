#pragma once

#include <luisa/std.hpp>
#include <luisa/resources.hpp>
using namespace luisa::shader;

static void clamp_radiance(float3& radiance, float exposure) {
	auto max_col = max(1e-4f, max(radiance.x, max(radiance.y, radiance.z)));
	radiance /= max_col;
	max_col = tanh(max_col / exposure) * exposure;
	radiance *= max_col;
}

inline float3 realtime_clamp_radiance(
	float3 radiance,
	Buffer<float>& exposure_buffer) {
	float ave_lum = exposure_buffer.read(2);
	float self_lum = max(radiance.x, max(radiance.y, radiance.z));
	float threshold = max(ave_lum * 128.f, 16.f);
	float new_lum = tanh(self_lum / threshold) * threshold;
	return radiance / max(self_lum, 1e-4f) * new_lum;
}
