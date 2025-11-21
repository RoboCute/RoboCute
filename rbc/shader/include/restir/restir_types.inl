// This struct included by both host and shader
#pragma once
#ifndef RESTIR_HOST
#include <luisa/std.hpp>
using namespace luisa::shader;
#endif
struct PackedReservoir {
	float targetPdf;
	uint lightSampledIndex;
	uint lightSampledUV;
	uint tlasIndex;
#ifndef RESTIR_HOST
	void Set(uint4 v) {
		targetPdf = bit_cast<float>(v.x);
		lightSampledIndex = v.y;
		lightSampledUV = v.z;
		tlasIndex = v.w;
	}
	uint4 Get() {
		uint4 r;
		r.x = bit_cast<uint>(targetPdf);
		r.y = lightSampledIndex;
		r.z = lightSampledUV;
		r.w = tlasIndex;
		return r;
	}
	float2 GetLightUV() {
		return float2((lightSampledUV & 0xffff) / float(0xffff), (lightSampledUV >> 16) / float(0xffff));
	}

	void SetLightUV(float2 lightUV) {
		lightSampledUV = (uint(lightUV.x * float(0xffff)) & 0xffff) + (uint(lightUV.y * float(0xffff)) << 16);
	}
#endif
};