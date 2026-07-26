#pragma once

#include <utils/shader_host.hpp>

#ifdef __SHADER_LANG__
#include <luisa/std.hpp>
#else
#include <cstdint>
#endif

namespace fsd {
SHADER_CODE(using namespace luisa::shader;)
HOST_CODE(using uint = uint32_t;)

constexpr float sigma_wavelength_factor = 25.0f;
constexpr float query_radius_sigma_factor = 3.0f;
constexpr float nanometers_to_meters = 1e-9f;
constexpr float world_units_per_meter = 1.0f;
constexpr float nanometers_to_world_scale =
    nanometers_to_meters * world_units_per_meter;
constexpr float query_radius_wavelength_factor =
    sigma_wavelength_factor * query_radius_sigma_factor;
constexpr uint geometry_visibility_mask = 0xffu;

constexpr float query_radius_from_wavelength_nm(float wavelength_nm) {
    return wavelength_nm * nanometers_to_world_scale *
        query_radius_wavelength_factor;
}

struct BufferIndices {
    uint edge_adjacency;
    uint instance_adjacency_offsets;
    uint inverse_cdf_lut;
};

}// namespace fsd
