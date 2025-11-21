#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;
struct GBuffer {
	std::array<float, 4> hitpos_normal;
	std::array<float, 3> beta;
	std::array<float, 3> radiance;
};