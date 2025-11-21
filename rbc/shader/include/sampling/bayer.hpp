#pragma once
#include <luisa/std.hpp>
namespace sampling {
using namespace luisa::shader;
struct BayerSampler {
	uint state;

	BayerSampler(uint2 p) {
		//first, spread bits
		p = (p ^ (p << 8u)) & 0x00ff00ffu;
		p = (p ^ (p << 4u)) & 0x0f0f0f0fu;
		p = (p ^ (p << 2u)) & 0x33333333u;
		p = (p ^ (p << 1u)) & 0x55555555u;

		//interleave with bayer bit order
		state = ((p.x ^ p.y) | (p.x << 1u));
	}
	float next() {
		auto tmp = float(reverse(state)) * 2.3283064365386963e-10f;
		uint lcg_a = 1103515245u;
		uint lcg_c = 12345u;
		state = lcg_a * state + lcg_c;
		return tmp;
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
}// namespace sampling