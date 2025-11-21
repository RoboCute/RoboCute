#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

using namespace luisa::shader;

template<BSDF T1, BSDF T2, FloatWeight W>
class MixingBSDF {
	T1 t1;
	T2 t2;
	W weight;
	float t1_sample_weight = 0.0f;
	float t2_sample_weight = 0.0f;

public:
	static constexpr BSDFFlags const flags = T1::flags | T2::flags;

	MixingBSDF() = default;

	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		weight.init(bp, p, data);
		if (weight.value() < 1.0f) {
			t1.init(wi, bp, p, data);
		}
		if (weight.value() > 0.0f) {
			t2.init(wi, bp, p, data);
		}
		if (weight.value() == 0.0f) {
			t1_sample_weight = 1.0f;
		} else if (weight.value() == 1.0f) {
			t2_sample_weight = 1.0f;
		} else {
			t1_sample_weight = (1.0f - weight.value()) * spectrum::spectrum_to_weight(t1.energy(wi, data));
			t2_sample_weight = weight.value() * spectrum::spectrum_to_weight(t2.energy(wi, data));
		}
		auto sum_weight = t1_sample_weight + t2_sample_weight;
		if (sum_weight <= 0.0f) return;
		t1_sample_weight /= sum_weight;
		t2_sample_weight /= sum_weight;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result;
		if (weight.value() < 1.0f) {
			result = t1.eval(wi, wo, data);
		}
		if (weight.value() > 0.0f) {
			result *= 1.0f - weight.value();
			result += t2.eval(wi, wo, data) * weight.value();
		}
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		if (t1_sample_weight + t2_sample_weight <= 0.0f) return BSDFSample{};

		if (data.rand.z <= t2_sample_weight) {
			data.rand.z /= t2_sample_weight;
			auto t2_sample = t2.sample(wi, data, volume_stack);
			t2_sample.throughput *= weight.value();
			t2_sample.pdf *= t2_sample_weight;
			return t2_sample;
		}
		data.rand.z = (data.rand.z - t2_sample_weight) / t1_sample_weight;
		auto t1_sample = t1.sample(wi, data, volume_stack);
		t1_sample.throughput *= (1.0f - weight.value());
		t1_sample.pdf *= t1_sample_weight;
		return t1_sample;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		float t1_pdf = 0.0f;
		float t2_pdf = 0.0f;
		if (weight.value() < 1.0f) {
			t1_pdf = t1.pdf(wi, wo, data);
		}
		if (weight.value() > 0.0f) {
			t2_pdf = t2.pdf(wi, wo, data);
		}
		if (t1_sample_weight + t2_sample_weight <= 0.0f)
			return 0.0f;
		return lerp(t1_pdf, t2_pdf, t2_sample_weight);
	}
	float3 tint_out(float3 wo, auto& data) const {
		float3 result = 0.0f;
		if (weight.value() < 1.0f) {
			result = t1.tint_out(wo, data);
		}
		if (weight.value() > 0.0f) {
			result *= 1.0f - weight.value();
			result += t2.tint_out(wo, data) * weight.value();
		}
		return result;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		float result = 0.0f;
		if (weight.value() < 1.0f) {
			result = t1.trans(wi, data, base_energy);
		}
		if (weight.value() > 0.0f) {
			result *= 1.0f - weight.value();
			result += t2.trans(wi, data, base_energy) * weight.value();
		}
		return result;
	}
	float3 energy(float3 wi, auto& data) const {
		float3 result = 0.0f;
		if (weight.value() < 1.0f) {
			result = t1.energy(wi, data);
		}
		if (weight.value() > 0.0f) {
			result *= 1.0f - weight.value();
			result += t2.energy(wi, data) * weight.value();
		}
		return result;
	}
};

}// namespace mtl