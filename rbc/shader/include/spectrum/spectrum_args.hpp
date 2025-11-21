#pragma once
#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
using namespace luisa::shader;
#else
namespace spectrum {
using luisa::float3;
using luisa::inv_pi;
using luisa::uint3;
}// namespace spectrum
#endif
namespace spectrum {
struct SpectrumArg {
	float3 lambda;
	uint hero_index;
	bool selected_wavelength = false;
};
constexpr uint spectrum_lut3d_res = 64;
// constexpr uint3 srgb_to_fourier_even_size(128, 128, 128);
constexpr uint cie_xyz_cdfinv_size = 2048;
// constexpr uint bmese_phase_size = 95;
constexpr uint illum_d65_size = 48;
constexpr float fourier_cmin = -inv_pi;
constexpr float fourier_cmax = inv_pi;
constexpr const float wavelength_min = 360;
constexpr const float wavelength_max = 830;
}// namespace spectrum
using spectrum::SpectrumArg;