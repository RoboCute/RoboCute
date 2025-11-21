#pragma once
#include <luisa/std.hpp>
#include <utils/onb.hpp>
#include <spectrum/spectrum.hpp>

namespace mtl {

enum class DiffractionGratingType : int {
	Sinusoidal = 0,
	Rectangular = 1,
	Linear = 2,
};

namespace openpbr {

using namespace luisa::shader;

struct Parameter {
	struct Weight {
		float base = 1.0f;
		float diffuse_roughness = 0.0f;
		float specular = 1.0f;
		float metalness = 0.0f;
		float subsurface = 0.0f;
		float transmission = 0.0f;
		float coat = 0.0f;
		float fuzz = 0.0f;
		float thin_film = 0.0f;
		float diffraction = 0.0f;
	} weight;

	struct Geometry {
		bool thin_walled = false;
		float thickness = 0.0f;// m
		Onb onb;
	} geometry;

	struct Specular {
		spectrum::SpectrumColor color = 1.0f;
		float roughness = 1.0f;
		float roughness_anisotropy = 0.0f;
		float ior = 1.5f;
	} specular;

	struct Emission {
		spectrum::SpectrumColor luminance = 0.0f;// nits
	} emission;

	struct Base {
		spectrum::SpectrumColor color = 0.8f;
	} base;

	struct Subsurface {
		spectrum::SpectrumColor color;
		float3 radius;// length
		float scatter_anisotropy;
	} subsurface;

	struct Transmission {
		spectrum::SpectrumColor color;
		float depth;// length
		float3 scatter;
		float scatter_anisotropy;
		float dispersion_scale;
		float dispersion_abbe_number;
	} transmission;

	struct Coat {
		spectrum::SpectrumColor color;
		float roughness;
		float roughness_anisotropy;
		float ior;
		float darkening;
		float roughening;// custom
		Onb coat_onb;
	} coat;

	struct Fuzz {
		spectrum::SpectrumColor color;
		float roughness;
	} fuzz;

	struct ThinFilm {
		float thickness;// μm
		float ior;
	} thin_film;

	struct Diffraction {// custom
		spectrum::SpectrumColor color;
		float thickness;// μm
		float2 inv_pitch;
		float angle;
		uint lobe_count;
		DiffractionGratingType type;
	} diffraction;
};

struct BasicParameter {
	Parameter::Weight weight;
	Parameter::Geometry geometry;
	Parameter::Specular specular;
	Parameter::Emission emission;
};

}// namespace openpbr
}// namespace mtl