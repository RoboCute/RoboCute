#pragma once

#include <utils/shader_host.hpp>
namespace material {
struct OpenPBR {
	struct Weight {
		float base{1.0f};
		float diffuse_roughness{0.0f};
		float specular{1.0f};
		float metallic{0.0f};
		MatImageHandle metallic_roughness_tex;
		float subsurface{0.0f};
		float transmission{0.0f};
		float thin_film{0.0f};
		float fuzz{0.0f};
		float coat{0.0f};
		float diffraction{0.0f};
	} weight;

	struct Geometry {
		float cutout_threshold{0.3f};
		float opacity{1.0f};
		MatImageHandle opacity_tex;
		float thickness{0.5f};// cm
		bool thin_walled{true};
		int nested_priority{0};
		float bump_scale{1.0f};
		MatImageHandle normal_tex;
	} geometry;

	struct UVs {
		std::array<float, 2> uv_scale{1.0f, 1.0f};
		std::array<float, 2> uv_offset{0.0f, 0.0f};
	} uvs;

	struct Specular {
		std::array<float, 3> specular_color{1.0f, 1.0f, 1.0f};
		float roughness{0.3f};
		float roughness_anisotropy{0.0f};
		MatImageHandle specular_anisotropy_level_tex;
		float roughness_anisotropy_angle{0.0f};// radians
		MatImageHandle specular_anisotropy_angle_tex;
		float ior{1.5f};
	} specular;

	struct Emission {
		std::array<float, 3> luminance{0.0f, 0.0f, 0.0f};
		MatImageHandle emission_tex;
	} emission;

	struct Base {
		std::array<float, 3> albedo{1.0f, 1.0f, 1.0f};
		MatImageHandle albedo_tex;
	} base;

	struct Subsurface {
		std::array<float, 3> subsurface_color{0.8f, 0.8f, 0.8f};
		float subsurface_radius{0.05f};
		std::array<float, 3> subsurface_radius_scale{1.0f, 0.5f, 0.25f};
		float subsurface_scatter_anisotropy{0.0f};
	} subsurface;

	struct Transmission {
		std::array<float, 3> transmission_color{1.0f, 1.0f, 1.0f};
		float transmission_depth{0.0f};
		std::array<float, 3> transmission_scatter{0.0f, 0.0f, 0.0f};
		float transmission_scatter_anisotropy{0.0f};
		float transmission_dispersion_scale{0.0f};
		float transmission_dispersion_abbe_number{20.0f};
	} transmission;

	struct Coat {
		std::array<float, 3> coat_color{1.0f, 1.0f, 1.0f};
		float coat_roughness{0.0f};
		float coat_roughness_anisotropy{0.0f};
		float coat_roughness_anisotropy_angle{0.0f};// radians
		float coat_ior{1.6f};
		float coat_darkening{1.0f};
		float coat_roughening{1.0f};
	} coat;

	struct Fuzz {
		std::array<float, 3> fuzz_color{1.0f, 1.0f, 1.0f};
		float fuzz_roughness{0.5f};
	} fuzz;

	struct Diffraction {
		std::array<float, 3> diffraction_color{1.0f, 1.0f, 1.0f};
		float diffraction_thickness{0.5f};// μm
		float diffraction_inv_pitch_x{1.0f / 3.0f};
		float diffraction_inv_pitch_y{0.0f};
		float diffraction_angle{0.0f};// radians
		uint diffraction_lobe_count{5};
		uint diffraction_type{1};// 0: Sinusoidal, 1: Rectangular, 2: Linear
	} diffraction;

	struct ThinFilm {
		float thin_film_thickness{0.5f};// μm
		float thin_film_ior{1.4f};
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
