#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf_flags.hpp>
#include <bsdfs/base/throughput.hpp>

namespace mtl {

using namespace luisa::shader;

struct BSDFSample {
	BSDFSample() = default;

	BSDFSample(float3 wo, float pdf)
		: wo(wo),
		  pdf(pdf) {}

	Throughput throughput;
	float pdf = 0.0f;
	float3 wo = 0.0f;

	constexpr
	operator bool() const {
		return static_cast<bool>(throughput);
	}
};

}// namespace mtl