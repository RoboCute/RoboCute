#pragma once
#include <luisa/std.hpp>

namespace mtl {

using namespace luisa::shader;

enum class BSDFFlags {
	None = 0,

	DiffuseReflection = 1 << 0,
	DiffuseTransmission = 1 << 1,
	SpecularReflection = 1 << 2,
	SpecularTransmission = 1 << 3,
	DeltaReflection = 1 << 4,
	DeltaTransmission = 1 << 5,

	Diffuse = DiffuseReflection | DiffuseTransmission,
	Specular = SpecularReflection | SpecularTransmission,
	Delta = DeltaReflection | DeltaTransmission,

	Reflection = DiffuseReflection | SpecularReflection | DeltaReflection,
	Transmission = DiffuseTransmission | SpecularTransmission | DeltaTransmission,

	All = Reflection | Transmission
};

constexpr BSDFFlags operator|(BSDFFlags a, BSDFFlags b) {
	return static_cast<BSDFFlags>(static_cast<int>(a) | static_cast<int>(b));
}
constexpr int operator&(BSDFFlags a, BSDFFlags b) {
	return (static_cast<int>(a) & static_cast<int>(b));
}
constexpr void operator|=(BSDFFlags& a, BSDFFlags b) {
	a = a | b;
}
constexpr void operator&=(BSDFFlags& a, BSDFFlags b) {
	a = BSDFFlags(a & b);
}
constexpr bool is_reflective(BSDFFlags f) {
	return f & BSDFFlags::Reflection;
}
constexpr bool is_transmissive(BSDFFlags f) {
	return f & BSDFFlags::Transmission;
}
constexpr bool is_diffuse(BSDFFlags f) {
	return f & BSDFFlags::Diffuse;
}
constexpr bool is_non_diffuse(BSDFFlags f) {
	return f & (BSDFFlags::Delta | BSDFFlags::Specular);
}
constexpr bool is_specular(BSDFFlags f) {
	return f & BSDFFlags::Specular;
}
constexpr bool is_delta(BSDFFlags f) {
	return f & BSDFFlags::Delta;
}
constexpr bool is_non_delta(BSDFFlags f) {
	return f & (BSDFFlags::Diffuse | BSDFFlags::Specular);
}

enum class ShadingDetail {
	Default,
	IndirectSpecular,
	IndirectDiffuse,
};

}// namespace mtl