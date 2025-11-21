#pragma once

#include <std/ex/type_list.hpp>

#include <bsdfs/openpbr.hpp>

#include <std/utility>
#define RBC_LITE_PBR_MATERIAL
namespace mtl {

enum class PolymorphicBSDFType {
#ifndef RBC_LITE_PBR_MATERIAL
	Openpbr,
#endif
	BasicMetallic,
// Basic,
// Metal,
#ifndef RBC_LITE_PBR_MATERIAL
	Subsurface,
#endif
	Glass,
	Count,
};

struct OpenpbrExtraParameter {
	openpbr::Parameter::Base base;
	openpbr::Parameter::Subsurface subsurface;
	openpbr::Parameter::Transmission transmission;
	openpbr::Parameter::Coat coat;
	openpbr::Parameter::Fuzz fuzz;
	openpbr::Parameter::ThinFilm thin_film;
	openpbr::Parameter::Diffraction diffraction;
};
struct BasicMetallicExtraParameter {
	openpbr::Parameter::Base base;
};
struct BasicExtraParameter {
	openpbr::Parameter::Base base;
};
struct MetalExtraParameter {
	openpbr::Parameter::Base base;
};
struct SubsurfaceExtraParameter {
	openpbr::Parameter::Subsurface subsurface;
};
struct GlassExtraParameter {
	openpbr::Parameter::Transmission transmission;
};

using PolymorphicBSDF = stdex::type_list<
#ifndef RBC_LITE_PBR_MATERIAL
	std::pair<OpenpbrBSDF, OpenpbrExtraParameter>,
#endif
	std::pair<MixingBSDF<WeightedLayeringBSDF<SmoothLambertianBRDF, DielectricSpecularBRDF, SpecularWeight>, ConductorBRDF, MetalnessWeight>, BasicMetallicExtraParameter>
// std::pair<WeightedLayeringBSDF<SmoothLambertianBRDF, DielectricSpecularBRDF, SpecularWeight>, BasicExtraParameter>,
// std::pair<ConductorBRDF, MetalExtraParameter>
#ifndef RBC_LITE_PBR_MATERIAL
	,
	std::pair<WeightedLayeringBSDF<SubsurfaceBSDF, DielectricSpecularBRDF, SpecularWeight>, SubsurfaceExtraParameter>
#endif
	,
	std::pair<TransmissionBSDF, GlassExtraParameter>>;

inline PolymorphicBSDFType detect_polymorphic_bsdf_type(
	auto const& weight) noexcept {
	mtl::PolymorphicBSDFType bsdf_type =
#ifndef RBC_LITE_PBR_MATERIAL
		mtl::PolymorphicBSDFType::Openpbr;
#else
		mtl::PolymorphicBSDFType::BasicMetallic;
#endif
	flatten();
	if (weight.diffraction == 0.0f &&
		weight.thin_film == 0.0f &&
		weight.coat == 0.0f &&
		weight.fuzz == 0.0f &&
		weight.diffuse_roughness == 0.0f) {
		flatten();
		// if (weight.metalness == 1.0f) {
		// 	bsdf_type = mtl::PolymorphicBSDFType::Metal;
		// } else {
		if (weight.metalness == 0.0f) {
			flatten();
			if (weight.transmission == 0.0f) {
				flatten();
#ifndef RBC_LITE_PBR_MATERIAL
				if (weight.subsurface == 1.0f) {
					bsdf_type = mtl::PolymorphicBSDFType::Subsurface;
				}
				// else
#endif
				// {
				// 	flatten();
				// if (weight.subsurface == 0.0f) {
				// 	bsdf_type = mtl::PolymorphicBSDFType::Basic;
				// }
				// }
			} else {
				flatten();
				if (weight.transmission == 1.0f) {
					bsdf_type = mtl::PolymorphicBSDFType::Glass;
				}
			}
		}
#ifndef RBC_LITE_PBR_MATERIAL
		else {
			flatten();
			if (weight.subsurface == 0.0f && weight.transmission == 0.0f) {
				bsdf_type = mtl::PolymorphicBSDFType::BasicMetallic;
			}
		}
#endif
	}
	return bsdf_type;
}

static_assert(PolymorphicBSDF::size == std::to_underlying(PolymorphicBSDFType::Count),
			  "PolymorphicBSDFType size mismatch");

}// namespace mtl
