#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/layering.hpp>
#include <bsdfs/base/mixing.hpp>
#include <bsdfs/mono/coat.hpp>
#include <bsdfs/mono/conductor.hpp>
#include <bsdfs/mono/diffraction_grating.hpp>
#include <bsdfs/mono/diffuse.hpp>
#include <bsdfs/mono/fuzz.hpp>
#include <bsdfs/mono/specular.hpp>
#include <bsdfs/mono/subsurface.hpp>
#include <bsdfs/mono/transmission.hpp>

namespace mtl {

using namespace luisa::shader;

class SpecularWeight {
	bool weight;

public:
	SpecularWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.specular > 0;
	}

	constexpr float value() const { return weight; }
};

class SubsurfaceWeight {
	float weight;

public:
	SubsurfaceWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.subsurface;
	}

	float value() const { return weight; }
};

class TransmissionWeight {
	float weight;

public:
	TransmissionWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.transmission;
	}

	float value() const { return weight; }
};

class MetalnessWeight {
	float weight;

public:
	MetalnessWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.metalness;
	}

	float value() const { return weight; }
};

class DiffractionWeight {
	float weight;

public:
	DiffractionWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.diffraction;
	}

	float value() const { return weight; }
};

class CoatWeight {
	float weight;

public:
	CoatWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.coat;
	}

	float value() const { return weight; }
};

class FuzzWeight {
	float weight;

public:
	FuzzWeight() = default;

	void init(auto const& bp, auto const& p, auto& data) {
		weight = bp.weight.fuzz;
	}

	float value() const { return weight; }
};

using MixedDiffuseBSDF = MixingBSDF<DiffuseBRDF, SubsurfaceBSDF, SubsurfaceWeight>;

using GlossyDiffuseBSDF = WeightedLayeringBSDF<MixedDiffuseBSDF, DielectricSpecularBRDF, SpecularWeight>;

using DielectricBaseBSDF = MixingBSDF<GlossyDiffuseBSDF, TransmissionBSDF, TransmissionWeight>;

using BaseSubstrateBSDF = MixingBSDF<DielectricBaseBSDF, ConductorBRDF, MetalnessWeight>;

using DiffractionBaseBSDF = WeightedLayeringBSDF<BaseSubstrateBSDF, DiffractionGratingBRDF, DiffractionWeight>;

using CoatedBaseBSDF = WeightedLayeringBSDF<DiffractionBaseBSDF, DielectricCoatBRDF, CoatWeight>;

using SurfaceBSDF = WeightedLayeringBSDF<CoatedBaseBSDF, FuzzBRDF, FuzzWeight>;

using OpenpbrBSDF = SurfaceBSDF;

}// namespace mtl