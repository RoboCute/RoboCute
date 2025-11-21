#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

using namespace luisa::shader;

template<BSDF Sub, BSDF Coat, FloatWeight W>
class WeightedLayeringBSDF {
	Sub sub;
	Coat coat;
	W weight;
	float3 coat_trans_in;
	float sub_sample_weight = 1.0f;
	float coat_sample_weight = 0.0f;

public:
	static constexpr BSDFFlags const flags = Sub::flags | Coat::flags;

	WeightedLayeringBSDF() = default;

	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		weight.init(bp, p, data);
		sub.init(wi, bp, p, data);
		if (weight.value() == 0.0f) return;
		coat.init(wi, bp, p, data);
		auto sub_energy = sub.energy(wi, data);
		coat_trans_in = coat.trans(wi, data, sub_energy);
		sub_sample_weight = spectrum::spectrum_to_weight(lerp(float3(1.0f), coat_trans_in, weight.value()) * sub_energy);
		coat_sample_weight = weight.value() * spectrum::spectrum_to_weight(coat.energy(wi, data));
		auto sum_weight = sub_sample_weight + coat_sample_weight;
		if (sum_weight <= 0.0f) return;
		sub_sample_weight /= sum_weight;
		coat_sample_weight /= sum_weight;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result = sub.eval(wi, wo, data);
		if (weight.value() > 0) {
			result *= lerp(float3(1.0f), coat_trans_in * coat.tint_out(wo, data), weight.value());
			result += coat.eval(wi, wo, data) * weight.value();
		}
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		if (sub_sample_weight + coat_sample_weight <= 0.0f)
			return BSDFSample{};
		if (data.rand.z <= coat_sample_weight) {
			data.rand.z /= coat_sample_weight;
			auto coat_sample = coat.sample(wi, data, volume_stack);
			coat_sample.throughput *= weight.value();
			coat_sample.pdf *= coat_sample_weight;
			return coat_sample;
		}
		data.rand.z = (data.rand.z - coat_sample_weight) / sub_sample_weight;
		auto sub_sample = sub.sample(wi, data, volume_stack);
		sub_sample.throughput *= lerp(float3(1.0f), coat_trans_in * coat.tint_out(sub_sample.wo, data), weight.value());
		sub_sample.pdf *= sub_sample_weight;
		return sub_sample;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		float sub_pdf = sub.pdf(wi, wo, data);
		if (weight.value() == 0.0f) {
			return sub_pdf;
		}
		if (sub_sample_weight + coat_sample_weight <= 0.0f)
			return 0.0f;
		return lerp(sub_pdf, coat.pdf(wi, wo, data), coat_sample_weight);
	}
	float3 tint_out(float3 wo, auto& data) const {
		auto sub_result = sub.tint_out(wo, data);
		if (weight.value() == 0.0f) {
			return sub_result;
		}
		return sub_result * lerp(float3(1.0f), coat.tint_out(wo, data), float3(weight.value()));
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		auto sub_result = sub.trans(wi, data, base_energy);
		if (weight.value() == 0.0f) {
			return sub_result;
		}
		return sub_result * lerp(1.0f, coat.trans(wi, data, base_energy), weight.value());
	}
	float3 energy(float3 wi, auto& data) const {
		auto sub_energy = sub.energy(wi, data);
		if (weight.value() == 0.0f) {
			return sub_energy;
		}
		return coat.energy(wi, data) * weight.value() +
			   sub_energy * lerp(float3(1.0f), coat_trans_in, float3(weight.value()));
	}
};

}// namespace mtl