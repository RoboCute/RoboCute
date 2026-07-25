#pragma once
#include <luisa/std.hpp>
#include <material/openpbr_params.hpp>
#include <luisa/printer.hpp>
#include <std/inplace_vector>

// #define DEBUG

namespace mtl {

using namespace luisa::shader;

struct Volume {
	static constexpr uint32 MAX_VOLUME_STACK_SIZE = 4u;

	float3 extinction;// units of inverse length
	float3 albedo;	  // single-scattering albedo
	float anisotropy; // phase function anisotropy in [-1, 1]
	float ior;
	uint boundary_id = max_uint32;
	int nested_priority = 0;

	void fill_from_transmission(float scatter_anisotropy, float3 scatter, float3 color, float depth) {
		if (depth > 0.0f) {
			float3 mu_t = -log(max(float3(1e-3f), color)) / depth;
			float3 mu_s = scatter / depth;
			float3 mu_a = mu_t - mu_s;
			mu_a -= float3(min(0.0f, reduce_min(mu_a)));
			extinction = mu_a + mu_s;
			albedo = ite(extinction != 0.0f, mu_s / extinction, float3(0));
			anisotropy = clamp(scatter_anisotropy, -0.99f, 0.99f);
		} else {
			extinction = 0.0f;
			albedo = 0.0f;
		}
	}
	void fill_from_subsurface(float scatter_anisotropy, float3 radius, float3 color) {
		float g = clamp(scatter_anisotropy, -0.95f, 0.95f);// scattering anisotropy
		float3 A = color;
		float3 s = 4.09712f + 4.20863f * A - sqrt(9.59217f + 41.6808f * A + 17.7126f * sqr(A));
		float3 s2 = sqr(s);
		extinction = 1.0f / max(float3(3e-4f), radius);
		albedo = (1.0f - s2) / max((1.0f - g * s2), float3(1e-6f));// single-scattering albedo accounting for anisotropy
		anisotropy = g;
	}

	void fill_from_transmission(openpbr::Parameter::Transmission const& p) {
		fill_from_transmission(
			p.scatter_anisotropy,
			p.scatter,
			p.color.spectral(),
			p.depth);
	}
	void fill_from_subsurface(openpbr::Parameter::Subsurface const& p) {
		fill_from_subsurface(
			p.scatter_anisotropy,
			p.radius,
			p.color.spectral());
	}
};

using ActiveMediumList = std::inplace_vector<Volume, Volume::MAX_VOLUME_STACK_SIZE>;

int active_medium_index(
	ActiveMediumList const& media,
	uint boundary_id) {
	if (media.empty()) return -1;
	if (media.back().boundary_id == boundary_id) return int(media.size() - 1u);
	if (media.size() == 2u && media[0].boundary_id == boundary_id) return 0;
	return -1;
}

bool active_medium_insert(
	ActiveMediumList& media,
	Volume const& medium) {
	if (media.size() >= Volume::MAX_VOLUME_STACK_SIZE) return false;
	if (media.empty()) {
		media.v[0] = medium;
		media.mSize = 1u;
		return true;
	}
	if (medium.nested_priority < media[0].nested_priority) {
		media.v[1] = media[0];
		media.v[0] = medium;
	} else {
		media.v[1] = medium;
	}
	media.mSize = 2u;
	return true;
}

bool active_medium_insert_enclosing(
	ActiveMediumList& media,
	Volume const& medium) {
	if (media.size() >= Volume::MAX_VOLUME_STACK_SIZE) return false;
	if (media.empty()) {
		media.v[0] = medium;
		media.mSize = 1u;
		return true;
	}
	if (medium.nested_priority <= media[0].nested_priority) {
		media.v[1] = media[0];
		media.v[0] = medium;
	} else {
		media.v[1] = medium;
	}
	media.mSize = 2u;
	return true;
}

bool active_medium_remove(
	ActiveMediumList& media,
	uint boundary_id) {
	if (media.empty()) return false;
	if (media.back().boundary_id == boundary_id) {
		media.pop_back();
		return true;
	}
	if (media.size() == 2u && media[0].boundary_id == boundary_id) {
		media.v[0] = media[1];
		media.mSize = 1u;
		return true;
	}
	return false;
}

bool active_medium_boundary_is_true(
	ActiveMediumList const& media,
	Volume const& boundary,
	bool entering) {
	if (entering) {
		return media.empty() || boundary.nested_priority >= media.back().nested_priority;
	}
	int found = active_medium_index(media, boundary.boundary_id);
	return found < 0 || uint(found) + 1u == media.size();
}

float active_medium_ior_across_boundary(
	ActiveMediumList const& media,
	uint boundary_id,
	bool entering) {
	if (media.empty()) return 1.0f;
	if (entering) return media.back().ior;
	if (media.back().boundary_id != boundary_id) return media.back().ior;
	return media.size() == 2u ? media[0].ior : 1.0f;
}

void collapse_active_medium_wavelengths(
	ActiveMediumList& media,
	uint hero_index) {
	if (!media.empty()) {
		media.v[0].extinction = media[0].extinction[hero_index];
		media.v[0].albedo = media[0].albedo[hero_index];
	}
	if (media.size() == 2u) {
		media.v[1].extinction = media[1].extinction[hero_index];
		media.v[1].albedo = media[1].albedo[hero_index];
	}
}

}// namespace mtl
