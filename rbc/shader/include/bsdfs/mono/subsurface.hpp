#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf.hpp>
#include <bsdfs/base/utils.hpp>

namespace mtl {

using namespace luisa::shader;

class SubsurfaceBSDF {
	spectrum::SpectrumColor color;
	float3 radius;
	float anisotropy;

public:
	static constexpr BSDFFlags const flags = BSDFFlags::Diffuse;

	SubsurfaceBSDF() = default;

	void init(float3 wi, auto const& bp, auto const& p, auto& data) {
		color = p.subsurface.color;
		anisotropy = p.subsurface.scatter_anisotropy;
		radius = p.subsurface.radius;
	}

	Throughput eval(float3 wi, float3 wo, auto& data) const {
		Throughput result;
		float reflect = wo.z * wi.z > 0.0f;
		if ((reflect && (!(data.sampling_flags & BSDFFlags::DiffuseReflection) ||
						 !data.geometry_thin_walled)) ||
			(!reflect && !(data.sampling_flags & BSDFFlags::DiffuseTransmission))) {
			return result;
		}
		result.val = inv_pi * abs(wo.z);

		if (data.geometry_thin_walled) result.val *= color.spectral() * 0.5f * (reflect ? (1.0f - anisotropy) : (1.0f + anisotropy));

		result.flags = reflect ? BSDFFlags::DiffuseReflection : BSDFFlags::DiffuseTransmission;
		return result;
	}
	BSDFSample sample(float3 wi, auto& data, auto& volume_stack) const {
		BSDFSample result;
		if (!(data.sampling_flags & BSDFFlags::Diffuse)) {
			return result;
		}
		float pr = data.geometry_thin_walled ? (1.0f - anisotropy) * 0.5f : 0.0f;
		float pt = data.geometry_thin_walled ? (1.0f + anisotropy) * 0.5f : 1.0f;
		if (!is_reflective(data.sampling_flags)) {
			pr = 0.0f;
		}
		if (!is_transmissive(data.sampling_flags)) {
			pt = 0.0f;
		}
		if (pr == 0.0f && pt == 0.0f)
			return result;

		result.wo = cosine_sample_hemisphere(data.rand.xy);
		result.pdf = inv_pi * result.wo.z;
		result.throughput.val = inv_pi * result.wo.z;
		if (wi.z < 0.0f)
			result.wo.z = -result.wo.z;
		if (data.rand.z <= pt) {
			result.wo.z = -result.wo.z;
			result.pdf *= pt;
			result.throughput.val *= pt;
			result.throughput.flags = BSDFFlags::DiffuseTransmission;
			if (!data.geometry_thin_walled) {
				if (wi.z >= 0.0f) {
					if (volume_stack.try_emplace_back()) {
						float temp_anisotropy = anisotropy;
						float3 temp_radius = radius;
						float3 temp_color = color.spectral();
						//TODO: fix compiler
						volume_stack.back([&](auto& back) {
							back.fill_from_subsurface(temp_anisotropy, temp_radius, temp_color);
							back.ior = data.original_ior;
						});
					}
				} else {
					if (volume_stack.empty()) {
						result.throughput.val *= color.spectral();
					} else {
						volume_stack.pop_back();
					}
				}
			} else {
				result.throughput.val *= color.spectral();
			}
		} else {
			result.pdf *= pr;
			result.throughput.val *= pr * color.spectral();
			result.throughput.flags = BSDFFlags::DiffuseReflection;
		}
		return result;
	}
	float pdf(float3 wi, float3 wo, auto& data) const {
		float pr = data.geometry_thin_walled ? (1.0f - anisotropy) * 0.5f : 0.0f;
		float pt = data.geometry_thin_walled ? (1.0f + anisotropy) * 0.5f : 1.0f;
		if (!is_reflective(data.sampling_flags)) {
			pr = 0.0f;
		}
		if (!is_transmissive(data.sampling_flags)) {
			pt = 0.0f;
		}
		if (pr == 0.0f && pt == 0.0f)
			return 0.0f;
		return inv_pi * abs(wo.z) * (wi.z * wo.z > 0.0f ? pr : pt);
	}
	float3 tint_out(float3 wo, auto& data) const {
		return 0.0f;
	}
	float3 trans(float3 wi, auto& data, float3 base_energy) const {
		return 0.0f;
	}
	float3 energy(float3 wi, auto& data) const {
		if (data.sampling_flags & BSDFFlags::DiffuseReflection) {
			if (!data.geometry_thin_walled) return 1.0f;
			return color.color(data.spectrumed);
		}
		return 0.0f;
	}
};

}// namespace mtl