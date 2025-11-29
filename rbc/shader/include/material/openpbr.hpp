#pragma once

#include <utils/shader_host.hpp>
namespace material {
#ifndef __SHADER_LANG__
luisa::half3 _make_half3_(float a, float b, float c) { return luisa::half3{(half)a, (half)b, (half)c} ;}
luisa::half4 _make_half4_(float a, float b, float c, float d) { return luisa::half4{(half)a, (half)b, (half)c, (half)d} ;}
#define MAKE_HALF3(...) _make_half3_(__VA_ARGS__)
#define MAKE_HALF4(...) _make_half4_(__VA_ARGS__)
#else
#define MAKE_HALF3(...) __VA_ARGS__
#define MAKE_HALF4(...) __VA_ARGS__
#endif
struct OpenPBR {
	struct Weight {
		MatImageHandle metallic_roughness_tex;
		half base{1.0f};
		half diffuse_roughness{0.0f};
		half specular{1.0f};
		half metallic{0.0f};
		half subsurface{0.0f};
		half transmission{0.0f};
		half thin_film{0.0f};
		half fuzz{0.0f};
		half coat{0.0f};
		half diffraction{0.0f};
	} weight;

	struct Geometry {
		half thickness{0.5f};// cm
		half cutout_threshold{0.3f};
		half opacity{1.0f};
		half bump_scale{1.0f};
		MatImageHandle opacity_tex;
		int nested_priority{0};
		MatImageHandle normal_tex;
		bool thin_walled{true};
	} geometry;

	struct UVs {
		std::array<float, 2> uv_scale{1.0f, 1.0f};
		std::array<float, 2> uv_offset{0.0f, 0.0f};
	} uvs;

	struct Specular {
		half4 specular_color_and_rough{MAKE_HALF4(1.0f, 1.0f, 1.0f, 0.3f)};
		half roughness_anisotropy{0.0f};
		half roughness_anisotropy_angle{0.0f};// radians
		half ior{1.5f};
		MatImageHandle specular_anisotropy_angle_tex;
		MatImageHandle specular_anisotropy_level_tex;
	} specular;

	struct Emission {
		std::array<float, 3> luminance{0.0f, 0.0f, 0.0f};
		MatImageHandle emission_tex;
	} emission;

	struct Base {
		half3 albedo{MAKE_HALF3(1,1,1)};
		MatImageHandle albedo_tex;
	} base;

	struct Subsurface {
		half4 subsurface_color_and_radius{MAKE_HALF4(0.8f, 0.8f, 0.8f, 0.05f)};
		half4 subsurface_radius_scale_andaniso{MAKE_HALF4(1.0f, 0.5f, 0.25f, 0.0f)};
	} subsurface;

	struct Transmission {
		half4 transmission_color_and_depth{MAKE_HALF4(1.0f, 1.0f, 1.0f, 0.0f)};
		half4 transmission_scatter_and_aniso{MAKE_HALF4(0.0f, 0.0f, 0.0f, 0.0f)};
		half transmission_dispersion_scale{0.0f};
		half transmission_dispersion_abbe_number{20.0f};
	} transmission;

	struct Coat {
		half4 coat_color_and_roughness{MAKE_HALF4(1.0f, 1.0f, 1.0f, 0.0f)};
		half coat_roughness_anisotropy{0.0f};
		half coat_roughness_anisotropy_angle{0.0f};// radians
		half coat_ior{1.6f};
		half coat_darkening{1.0f};
		half coat_roughening{1.0f};
	} coat;

	struct Fuzz {
		half4 fuzz_color_and_roughness{MAKE_HALF4(1.0f, 1.0f, 1.0f, 0.5f)};
	} fuzz;

	struct Diffraction {
		half4 diffraction_color_and_thickness{MAKE_HALF4(1.0f, 1.0f, 1.0f, 0.5f)};
		half diffraction_inv_pitch_x{1.0f / 3.0f};
		half diffraction_inv_pitch_y{0.0f};
		half diffraction_angle{0.0f};// radians
		uint diffraction_lobe_count{5};
		uint diffraction_type{1};// 0: Sinusoidal, 1: Rectangular, 2: Linear
	} diffraction;

	struct ThinFilm {
		half thin_film_thickness{0.5f};// μm
		half thin_film_ior{1.4f};
	} thin_film;

	SHADER_CODE(
		static bool cutout(
			BindlessBuffer& buffer_heap,
			BindlessImage& image_heap,
			uint mat_type,
			uint mat_index,
			vt::VTMeta vt_meta,
			float2 uv,
			int& priority,
			auto& rng);

		static float3 get_emission(
			BindlessBuffer & buffer_heap,
			BindlessImage& image_heap,
			uint mat_type,
			uint mat_index,
			float2 uv);

		static bool transform_to_params(
			BindlessBuffer& buffer_heap,
			BindlessImage& image_heap,
			uint mat_type,
			uint mat_index,
			auto& params,
			uint texture_filter,
			vt::VTMeta vt_meta,
			float2 uv,
			float4 ddxy,
			float3 input_dir,
			bool& reject,
			auto&&...);

	)
};
}// namespace material
