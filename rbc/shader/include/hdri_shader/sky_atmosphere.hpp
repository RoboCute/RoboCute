#pragma once
#include <luisa/std.hpp>
using namespace luisa::shader;
static float HenyeyGreensteinPhase(float G, float CosTheta) {
	// Reference implementation (i.e. not schlick approximation).
	// See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
	float Numer = 1.0f - G * G;
	float Denom = 1.0f + G * G + 2.0f * G * CosTheta;
	return Numer / (4.0f * pi * Denom * sqrt(Denom));
}

static float RayleighPhase(float CosTheta) {
	float Factor = 3.0f / (16.0f * pi);
	return Factor * (1.0f + CosTheta * CosTheta);
}