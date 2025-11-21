#pragma once
#include <luisa/std.hpp>
namespace sampling {
using namespace luisa::shader;
struct PCGSampler {

	static constexpr uint const PRIME32_2 = 2246822519u;
	static constexpr uint const PRIME32_3 = 3266489917u;
	static constexpr uint const PRIME32_4 = 668265263u;
	static constexpr uint const PRIME32_5 = 374761393u;

	uint state;
	PCGSampler(uint v) {
		uint h32 = v + PRIME32_5;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		state = h32 ^ (h32 >> 16);
	}
	PCGSampler(uint2 v) {
		uint h32 = v.y + PRIME32_5 + v.x * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		state = h32 ^ (h32 >> 16);
	}
	PCGSampler(uint3 v) {
		uint h32 = v.z + PRIME32_5 + v.x * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 += v.y * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		state = h32 ^ (h32 >> 16);
	}
	uint nextui() {
		uint old_state = state;
		state = state * 747796405u + 2891336453u;
		uint word = ((old_state >> ((old_state >> 28u) + 4u)) ^ old_state) * 277803737u;
		return (word >> 22u) ^ word;
	}
	float next() {
		return bit_cast<float>((nextui() >> 9u) | 0x3f800000u) - 1.0f;
	}
	float2 next2f() {
		return float2{next(), next()};
	}
	float3 next3f() {
		return float3{next(), next(), next()};
	}
	float next(auto&&) {
		return next();
	}
	float2 next2f(auto&&) {
		return next2f();
	}
	float3 next3f(auto&&) {
		return next3f();
	}
};
struct PCGSamplerOffsetted {
	PCGSampler sampler;
	float3 offset;
	PCGSamplerOffsetted(uint v) : sampler(v) {}
	PCGSamplerOffsetted(uint2 v) : sampler(v) {}
	PCGSamplerOffsetted(uint3 v) : sampler(v) {}
	uint nextui() {
		return sampler.nextui();
	}
	float next() {
		return fract(sampler.next() + offset.x);
	}
	float2 next2f() {
		return fract(sampler.next2f() + offset.xy);
	}
	float3 next3f() {
		return fract(sampler.next3f() + offset);
	}
	float next(auto&&) {
		return next();
	}
	float2 next2f(auto&&) {
		return next2f();
	}
	float3 next3f(auto&&) {
		return next3f();
	}
};
}// namespace sampling