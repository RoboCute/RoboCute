#include "generated/world.h"
#include <rbc_core/json_serde.h>
#include <rbc_core/rc.h>
#include <luisa/core/mathematics.h>
#include <rbc_world/resources/texture.h>
#include <rbc_graphics/materials.h>
#include <rbc_graphics/mat_serde.h>

namespace rbc {
#include <material/openpbr.hpp>
/**
 * @brief Internal implementation structure for OpenPBR material data.
 * 
 * This structure holds the actual material data using a variant type that can
 * store either texture references or scalar/vector values.
 */
struct OpenPBRImpl : RCBase {
    /**
     * @brief Variant type that can hold texture reference or scalar/vector types.
     * 
     * Supported types:
     * - Texture reference (RC<world::TextureResource>)
     * - Scalar types: float, int, uint32_t, bool
     * - Vector types: float2, float3, float4, int2, int3, int4, uint2, uint3, uint4
     */
    using ValueType = luisa::variant<
        RC<world::TextureResource>,
        float,
        int,
        uint32_t,
        bool,
        luisa::float2,
        luisa::float3,
        luisa::float4,
        luisa::int2,
        luisa::int3,
        luisa::int4,
        luisa::uint2,
        luisa::uint3,
        luisa::uint4>;

    /**
     * @brief Hash map storing material property values keyed by field name.
     * 
     * Field names follow the pattern: "<category>_<field>" (e.g., "weight_base", 
     * "base_albedo", "specular_roughness")
     */
    vstd::HashMap<vstd::string, ValueType> maps;
};

// Helper macros for getter/setter implementations
#define RBC_IMPL_GET_FLOAT(key, default_val)                            \
    do {                                                                \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                 \
        if (auto it = impl->maps.find(key); it) {                       \
            if (auto *v = luisa::get_if<float>(&it.value())) return *v; \
        }                                                               \
        return default_val;                                             \
    } while (0)

#define RBC_IMPL_GET_INT(key, default_val)                            \
    do {                                                              \
        auto *impl = static_cast<OpenPBRImpl *>(this_);               \
        if (auto it = impl->maps.find(key); it) {                     \
            if (auto *v = luisa::get_if<int>(&it.value())) return *v; \
        }                                                             \
        return default_val;                                           \
    } while (0)

#define RBC_IMPL_GET_UINT(key, default_val)                                \
    do {                                                                   \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                    \
        if (auto it = impl->maps.find(key); it) {                          \
            if (auto *v = luisa::get_if<uint32_t>(&it.value())) return *v; \
        }                                                                  \
        return default_val;                                                \
    } while (0)

#define RBC_IMPL_GET_BOOL(key, default_val)                            \
    do {                                                               \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                \
        if (auto it = impl->maps.find(key); it) {                      \
            if (auto *v = luisa::get_if<bool>(&it.value())) return *v; \
        }                                                              \
        return default_val;                                            \
    } while (0)

#define RBC_IMPL_GET_FLOAT2(key, default_val)                                   \
    do {                                                                        \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                         \
        if (auto it = impl->maps.find(key); it) {                               \
            if (auto *v = luisa::get_if<luisa::float2>(&it.value())) return *v; \
        }                                                                       \
        return default_val;                                                     \
    } while (0)

#define RBC_IMPL_GET_FLOAT3(key, default_val)                                   \
    do {                                                                        \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                         \
        if (auto it = impl->maps.find(key); it) {                               \
            if (auto *v = luisa::get_if<luisa::float3>(&it.value())) return *v; \
        }                                                                       \
        return default_val;                                                     \
    } while (0)

#define RBC_IMPL_GET_TEX(key)                                                       \
    do {                                                                            \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                             \
        if (auto it = impl->maps.find(key); it) {                                   \
            if (auto *v = luisa::get_if<RC<world::TextureResource>>(&it.value())) { \
                if (*v) {                                                           \
                    manually_add_ref(v->get());                                     \
                    return v->get();                                                \
                }                                                                   \
            }                                                                       \
        }                                                                           \
        return nullptr;                                                             \
    } while (0)

#define RBC_IMPL_SET_VALUE(key, type, value)                   \
    do {                                                       \
        auto *impl = static_cast<OpenPBRImpl *>(this_);        \
        impl->maps.try_emplace(key, static_cast<type>(value)); \
    } while (0)

#define RBC_IMPL_SET_TEX(key, value)                                      \
    do {                                                                  \
        auto *impl = static_cast<OpenPBRImpl *>(this_);                   \
        if (value) {                                                      \
            auto *tex = static_cast<world::TextureResource *>(value);     \
            impl->maps.try_emplace(key, RC<world::TextureResource>(tex)); \
        } else {                                                          \
            impl->maps.remove(key);                                       \
        }                                                                 \
    } while (0)

void *OpenPBRInterface::_create_() {
    auto ptr = new OpenPBRImpl();
    manually_add_ref(ptr);
    return ptr;
}

// Weight getters
float OpenPBRInterface::get_weight_base(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_base", 1.0f);
}

float OpenPBRInterface::get_weight_diffuse_roughness(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_diffuse_roughness", 0.0f);
}

float OpenPBRInterface::get_weight_specular(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_specular", 1.0f);
}

float OpenPBRInterface::get_weight_metallic(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_metallic", 0.0f);
}

void *OpenPBRInterface::get_weight_metallic_roughness_tex(void *this_) {
    RBC_IMPL_GET_TEX("weight_metallic_roughness_tex");
}

float OpenPBRInterface::get_weight_subsurface(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_subsurface", 0.0f);
}

float OpenPBRInterface::get_weight_transmission(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_transmission", 0.0f);
}

float OpenPBRInterface::get_weight_thin_film(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_thin_film", 0.0f);
}

float OpenPBRInterface::get_weight_fuzz(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_fuzz", 0.0f);
}

float OpenPBRInterface::get_weight_coat(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_coat", 0.0f);
}

float OpenPBRInterface::get_weight_diffraction(void *this_) {
    RBC_IMPL_GET_FLOAT("weight_diffraction", 0.0f);
}

// Geometry getters
float OpenPBRInterface::get_geometry_cutout_threshold(void *this_) {
    RBC_IMPL_GET_FLOAT("geometry_cutout_threshold", 0.3f);
}

float OpenPBRInterface::get_geometry_opacity(void *this_) {
    RBC_IMPL_GET_FLOAT("geometry_opacity", 1.0f);
}

void *OpenPBRInterface::get_geometry_opacity_tex(void *this_) {
    RBC_IMPL_GET_TEX("geometry_opacity_tex");
}

float OpenPBRInterface::get_geometry_thickness(void *this_) {
    RBC_IMPL_GET_FLOAT("geometry_thickness", 0.5f);
}

bool OpenPBRInterface::get_geometry_thin_walled(void *this_) {
    RBC_IMPL_GET_BOOL("geometry_thin_walled", true);
}

int32_t OpenPBRInterface::get_geometry_nested_priority(void *this_) {
    RBC_IMPL_GET_INT("geometry_nested_priority", 0);
}

float OpenPBRInterface::get_geometry_bump_scale(void *this_) {
    RBC_IMPL_GET_FLOAT("geometry_bump_scale", 1.0f);
}

void *OpenPBRInterface::get_geometry_normal_tex(void *this_) {
    RBC_IMPL_GET_TEX("geometry_normal_tex");
}

// UVs getters
luisa::float2 OpenPBRInterface::get_uvs_scale(void *this_) {
    RBC_IMPL_GET_FLOAT2("uv_scale", luisa::float2(1.0f, 1.0f));
}

luisa::float2 OpenPBRInterface::get_uvs_offset(void *this_) {
    RBC_IMPL_GET_FLOAT2("uv_offset", luisa::float2(0.0f, 0.0f));
}

// Specular getters
luisa::float3 OpenPBRInterface::get_specular_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("specular_color", luisa::float3(1.0f, 1.0f, 1.0f));
}

float OpenPBRInterface::get_specular_roughness(void *this_) {
    RBC_IMPL_GET_FLOAT("specular_roughness", 0.3f);
}

float OpenPBRInterface::get_specular_roughness_anisotropy(void *this_) {
    RBC_IMPL_GET_FLOAT("specular_roughness_anisotropy", 0.0f);
}

void *OpenPBRInterface::get_specular_anisotropy_level_tex(void *this_) {
    RBC_IMPL_GET_TEX("specular_anisotropy_level_tex");
}

float OpenPBRInterface::get_specular_roughness_anisotropy_angle(void *this_) {
    RBC_IMPL_GET_FLOAT("specular_roughness_anisotropy_angle", 0.0f);
}

void *OpenPBRInterface::get_specular_anisotropy_angle_tex(void *this_) {
    RBC_IMPL_GET_TEX("specular_anisotropy_angle_tex");
}

float OpenPBRInterface::get_specular_ior(void *this_) {
    RBC_IMPL_GET_FLOAT("specular_ior", 1.5f);
}

// Emission getters
luisa::float3 OpenPBRInterface::get_emission_luminance(void *this_) {
    RBC_IMPL_GET_FLOAT3("emission_luminance", luisa::float3(0.0f, 0.0f, 0.0f));
}

void *OpenPBRInterface::get_emission_emission_tex(void *this_) {
    RBC_IMPL_GET_TEX("emission_tex");
}

// Base getters
luisa::float3 OpenPBRInterface::get_base_albedo(void *this_) {
    RBC_IMPL_GET_FLOAT3("base_albedo", luisa::float3(1.0f, 1.0f, 1.0f));
}

void *OpenPBRInterface::get_base_albedo_tex(void *this_) {
    RBC_IMPL_GET_TEX("base_albedo_tex");
}

// Subsurface getters
luisa::float3 OpenPBRInterface::get_subsurface_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("subsurface_color", luisa::float3(0.8f, 0.8f, 0.8f));
}

float OpenPBRInterface::get_subsurface_radius(void *this_) {
    RBC_IMPL_GET_FLOAT("subsurface_radius", 0.05f);
}

luisa::float3 OpenPBRInterface::get_subsurface_radius_scale(void *this_) {
    RBC_IMPL_GET_FLOAT3("subsurface_radius_scale", luisa::float3(1.0f, 0.5f, 0.25f));
}

float OpenPBRInterface::get_subsurface_scatter_anisotropy(void *this_) {
    RBC_IMPL_GET_FLOAT("subsurface_scatter_anisotropy", 0.0f);
}

// Transmission getters
luisa::float3 OpenPBRInterface::get_transmission_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("transmission_color", luisa::float3(1.0f, 1.0f, 1.0f));
}

float OpenPBRInterface::get_transmission_depth(void *this_) {
    RBC_IMPL_GET_FLOAT("transmission_depth", 0.0f);
}

luisa::float3 OpenPBRInterface::get_transmission_scatter(void *this_) {
    RBC_IMPL_GET_FLOAT3("transmission_scatter", luisa::float3(0.0f, 0.0f, 0.0f));
}

float OpenPBRInterface::get_transmission_scatter_anisotropy(void *this_) {
    RBC_IMPL_GET_FLOAT("transmission_scatter_anisotropy", 0.0f);
}

float OpenPBRInterface::get_transmission_dispersion_scale(void *this_) {
    RBC_IMPL_GET_FLOAT("transmission_dispersion_scale", 0.0f);
}

float OpenPBRInterface::get_transmission_dispersion_abbe_number(void *this_) {
    RBC_IMPL_GET_FLOAT("transmission_dispersion_abbe_number", 20.0f);
}

// Coat getters
luisa::float3 OpenPBRInterface::get_coat_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("coat_color", luisa::float3(1.0f, 1.0f, 1.0f));
}

float OpenPBRInterface::get_coat_roughness(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_roughness", 0.0f);
}

float OpenPBRInterface::get_coat_roughness_anisotropy(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_roughness_anisotropy", 0.0f);
}

float OpenPBRInterface::get_coat_roughness_anisotropy_angle(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_roughness_anisotropy_angle", 0.0f);
}

float OpenPBRInterface::get_coat_ior(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_ior", 1.6f);
}

float OpenPBRInterface::get_coat_darkening(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_darkening", 1.0f);
}

float OpenPBRInterface::get_coat_roughening(void *this_) {
    RBC_IMPL_GET_FLOAT("coat_roughening", 1.0f);
}

// Fuzz getters
luisa::float3 OpenPBRInterface::get_fuzz_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("fuzz_color", luisa::float3(1.0f, 1.0f, 1.0f));
}

float OpenPBRInterface::get_fuzz_roughness(void *this_) {
    RBC_IMPL_GET_FLOAT("fuzz_roughness", 0.5f);
}

// Diffraction getters
luisa::float3 OpenPBRInterface::get_diffraction_color(void *this_) {
    RBC_IMPL_GET_FLOAT3("diffraction_color", luisa::float3(1.0f, 1.0f, 1.0f));
}

float OpenPBRInterface::get_diffraction_thickness(void *this_) {
    RBC_IMPL_GET_FLOAT("diffraction_thickness", 0.5f);
}

float OpenPBRInterface::get_diffraction_inv_pitch_x(void *this_) {
    RBC_IMPL_GET_FLOAT("diffraction_inv_pitch_x", 1.0f / 3.0f);
}

float OpenPBRInterface::get_diffraction_inv_pitch_y(void *this_) {
    RBC_IMPL_GET_FLOAT("diffraction_inv_pitch_y", 0.0f);
}

float OpenPBRInterface::get_diffraction_angle(void *this_) {
    RBC_IMPL_GET_FLOAT("diffraction_angle", 0.0f);
}

uint32_t OpenPBRInterface::get_diffraction_lobe_count(void *this_) {
    RBC_IMPL_GET_UINT("diffraction_lobe_count", 5);
}

uint32_t OpenPBRInterface::get_diffraction_type(void *this_) {
    RBC_IMPL_GET_UINT("diffraction_type", 1);
}

// ThinFilm getters
float OpenPBRInterface::get_thin_film_thickness(void *this_) {
    RBC_IMPL_GET_FLOAT("thin_film_thickness", 0.5f);
}

float OpenPBRInterface::get_thin_film_ior(void *this_) {
    RBC_IMPL_GET_FLOAT("thin_film_ior", 1.4f);
}

// Weight setters
void OpenPBRInterface::set_weight_base(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_base", float, value);
}

void OpenPBRInterface::set_weight_diffuse_roughness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_diffuse_roughness", float, value);
}

void OpenPBRInterface::set_weight_specular(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_specular", float, value);
}

void OpenPBRInterface::set_weight_metallic(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_metallic", float, value);
}

void OpenPBRInterface::set_weight_metallic_roughness_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("weight_metallic_roughness_tex", value);
}

void OpenPBRInterface::set_weight_subsurface(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_subsurface", float, value);
}

void OpenPBRInterface::set_weight_transmission(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_transmission", float, value);
}

void OpenPBRInterface::set_weight_thin_film(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_thin_film", float, value);
}

void OpenPBRInterface::set_weight_fuzz(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_fuzz", float, value);
}

void OpenPBRInterface::set_weight_coat(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_coat", float, value);
}

void OpenPBRInterface::set_weight_diffraction(void *this_, float value) {
    RBC_IMPL_SET_VALUE("weight_diffraction", float, value);
}

// Geometry setters
void OpenPBRInterface::set_geometry_cutout_threshold(void *this_, float value) {
    RBC_IMPL_SET_VALUE("geometry_cutout_threshold", float, value);
}

void OpenPBRInterface::set_geometry_opacity(void *this_, float value) {
    RBC_IMPL_SET_VALUE("geometry_opacity", float, value);
}

void OpenPBRInterface::set_geometry_opacity_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("geometry_opacity_tex", value);
}

void OpenPBRInterface::set_geometry_thickness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("geometry_thickness", float, value);
}

void OpenPBRInterface::set_geometry_thin_walled(void *this_, bool value) {
    RBC_IMPL_SET_VALUE("geometry_thin_walled", bool, value);
}

void OpenPBRInterface::set_geometry_nested_priority(void *this_, int32_t value) {
    RBC_IMPL_SET_VALUE("geometry_nested_priority", int, value);
}

void OpenPBRInterface::set_geometry_bump_scale(void *this_, float value) {
    RBC_IMPL_SET_VALUE("geometry_bump_scale", float, value);
}

void OpenPBRInterface::set_geometry_normal_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("geometry_normal_tex", value);
}

// UVs setters
void OpenPBRInterface::set_uvs_scale(void *this_, luisa::float2 value) {
    RBC_IMPL_SET_VALUE("uv_scale", luisa::float2, value);
}

void OpenPBRInterface::set_uvs_offset(void *this_, luisa::float2 value) {
    RBC_IMPL_SET_VALUE("uv_offset", luisa::float2, value);
}

// Specular setters
void OpenPBRInterface::set_specular_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("specular_color", luisa::float3, value);
}

void OpenPBRInterface::set_specular_roughness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("specular_roughness", float, value);
}

void OpenPBRInterface::set_specular_roughness_anisotropy(void *this_, float value) {
    RBC_IMPL_SET_VALUE("specular_roughness_anisotropy", float, value);
}

void OpenPBRInterface::set_specular_anisotropy_level_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("specular_anisotropy_level_tex", value);
}

void OpenPBRInterface::set_specular_roughness_anisotropy_angle(void *this_, float value) {
    RBC_IMPL_SET_VALUE("specular_roughness_anisotropy_angle", float, value);
}

void OpenPBRInterface::set_specular_anisotropy_angle_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("specular_anisotropy_angle_tex", value);
}

void OpenPBRInterface::set_specular_ior(void *this_, float value) {
    RBC_IMPL_SET_VALUE("specular_ior", float, value);
}

// Emission setters
void OpenPBRInterface::set_emission_luminance(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("emission_luminance", luisa::float3, value);
}

void OpenPBRInterface::set_emission_emission_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("emission_tex", value);
}

// Base setters
void OpenPBRInterface::set_base_albedo(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("base_albedo", luisa::float3, value);
}

void OpenPBRInterface::set_base_albedo_tex(void *this_, void *value) {
    RBC_IMPL_SET_TEX("base_albedo_tex", value);
}

// Subsurface setters
void OpenPBRInterface::set_subsurface_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("subsurface_color", luisa::float3, value);
}

void OpenPBRInterface::set_subsurface_radius(void *this_, float value) {
    RBC_IMPL_SET_VALUE("subsurface_radius", float, value);
}

void OpenPBRInterface::set_subsurface_radius_scale(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("subsurface_radius_scale", luisa::float3, value);
}

void OpenPBRInterface::set_subsurface_scatter_anisotropy(void *this_, float value) {
    RBC_IMPL_SET_VALUE("subsurface_scatter_anisotropy", float, value);
}

// Transmission setters
void OpenPBRInterface::set_transmission_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("transmission_color", luisa::float3, value);
}

void OpenPBRInterface::set_transmission_depth(void *this_, float value) {
    RBC_IMPL_SET_VALUE("transmission_depth", float, value);
}

void OpenPBRInterface::set_transmission_scatter(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("transmission_scatter", luisa::float3, value);
}

void OpenPBRInterface::set_transmission_scatter_anisotropy(void *this_, float value) {
    RBC_IMPL_SET_VALUE("transmission_scatter_anisotropy", float, value);
}

void OpenPBRInterface::set_transmission_dispersion_scale(void *this_, float value) {
    RBC_IMPL_SET_VALUE("transmission_dispersion_scale", float, value);
}

void OpenPBRInterface::set_transmission_dispersion_abbe_number(void *this_, float value) {
    RBC_IMPL_SET_VALUE("transmission_dispersion_abbe_number", float, value);
}

// Coat setters
void OpenPBRInterface::set_coat_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("coat_color", luisa::float3, value);
}

void OpenPBRInterface::set_coat_roughness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_roughness", float, value);
}

void OpenPBRInterface::set_coat_roughness_anisotropy(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_roughness_anisotropy", float, value);
}

void OpenPBRInterface::set_coat_roughness_anisotropy_angle(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_roughness_anisotropy_angle", float, value);
}

void OpenPBRInterface::set_coat_ior(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_ior", float, value);
}

void OpenPBRInterface::set_coat_darkening(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_darkening", float, value);
}

void OpenPBRInterface::set_coat_roughening(void *this_, float value) {
    RBC_IMPL_SET_VALUE("coat_roughening", float, value);
}

// Fuzz setters
void OpenPBRInterface::set_fuzz_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("fuzz_color", luisa::float3, value);
}

void OpenPBRInterface::set_fuzz_roughness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("fuzz_roughness", float, value);
}

// Diffraction setters
void OpenPBRInterface::set_diffraction_color(void *this_, luisa::float3 value) {
    RBC_IMPL_SET_VALUE("diffraction_color", luisa::float3, value);
}

void OpenPBRInterface::set_diffraction_thickness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("diffraction_thickness", float, value);
}

void OpenPBRInterface::set_diffraction_inv_pitch_x(void *this_, float value) {
    RBC_IMPL_SET_VALUE("diffraction_inv_pitch_x", float, value);
}

void OpenPBRInterface::set_diffraction_inv_pitch_y(void *this_, float value) {
    RBC_IMPL_SET_VALUE("diffraction_inv_pitch_y", float, value);
}

void OpenPBRInterface::set_diffraction_angle(void *this_, float value) {
    RBC_IMPL_SET_VALUE("diffraction_angle", float, value);
}

void OpenPBRInterface::set_diffraction_lobe_count(void *this_, uint32_t value) {
    RBC_IMPL_SET_VALUE("diffraction_lobe_count", uint32_t, value);
}

void OpenPBRInterface::set_diffraction_type(void *this_, uint32_t value) {
    RBC_IMPL_SET_VALUE("diffraction_type", uint32_t, value);
}

// ThinFilm setters
void OpenPBRInterface::set_thin_film_thickness(void *this_, float value) {
    RBC_IMPL_SET_VALUE("thin_film_thickness", float, value);
}

void OpenPBRInterface::set_thin_film_ior(void *this_, float value) {
    RBC_IMPL_SET_VALUE("thin_film_ior", float, value);
}
/**
 * @brief Visitor for serializing variant values to JSON.
 */
struct JsonValueWriter {
    JsonWriter &writer;
    char const *key;

    void operator()(float v) const { writer.add(static_cast<double>(v), key); }
    void operator()(int v) const { writer.add(static_cast<int64_t>(v), key); }
    void operator()(uint32_t v) const { writer.add(static_cast<uint64_t>(v), key); }
    void operator()(bool v) const { writer.add(v, key); }

    void operator()(luisa::float2 const &v) const {
        writer.start_array();
        writer.add(static_cast<double>(v.x));
        writer.add(static_cast<double>(v.y));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::float3 const &v) const {
        writer.start_array();
        writer.add(static_cast<double>(v.x));
        writer.add(static_cast<double>(v.y));
        writer.add(static_cast<double>(v.z));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::float4 const &v) const {
        writer.start_array();
        writer.add(static_cast<double>(v.x));
        writer.add(static_cast<double>(v.y));
        writer.add(static_cast<double>(v.z));
        writer.add(static_cast<double>(v.w));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::int2 const &v) const {
        writer.start_array();
        writer.add(static_cast<int64_t>(v.x));
        writer.add(static_cast<int64_t>(v.y));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::int3 const &v) const {
        writer.start_array();
        writer.add(static_cast<int64_t>(v.x));
        writer.add(static_cast<int64_t>(v.y));
        writer.add(static_cast<int64_t>(v.z));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::int4 const &v) const {
        writer.start_array();
        writer.add(static_cast<int64_t>(v.x));
        writer.add(static_cast<int64_t>(v.y));
        writer.add(static_cast<int64_t>(v.z));
        writer.add(static_cast<int64_t>(v.w));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::uint2 const &v) const {
        writer.start_array();
        writer.add(static_cast<uint64_t>(v.x));
        writer.add(static_cast<uint64_t>(v.y));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::uint3 const &v) const {
        writer.start_array();
        writer.add(static_cast<uint64_t>(v.x));
        writer.add(static_cast<uint64_t>(v.y));
        writer.add(static_cast<uint64_t>(v.z));
        writer.add_last_scope_to_object(key);
    }

    void operator()(luisa::uint4 const &v) const {
        writer.start_array();
        writer.add(static_cast<uint64_t>(v.x));
        writer.add(static_cast<uint64_t>(v.y));
        writer.add(static_cast<uint64_t>(v.z));
        writer.add(static_cast<uint64_t>(v.w));
        writer.add_last_scope_to_object(key);
    }

    void operator()(RC<world::TextureResource> const &tex) const {
        if (tex && tex->guid()) {
            writer.add(tex->guid(), key);
        }
    }
};

/**
 * @brief Serializes OpenPBR material data to JSON string.
 * 
 * @param this_ Pointer to OpenPBRImpl instance.
 * @return JSON string representation of material data.
 */
luisa::string OpenPBRInterface::dump_to_json(void *this_) {
    auto *impl = static_cast<OpenPBRImpl *>(this_);
    JsonWriter writer;

    for (auto &&kv : impl->maps) {
        luisa::visit(JsonValueWriter{writer, kv.first.c_str()}, kv.second);
    }

    luisa::string result;
    writer.write_to(result);
    return result;
}

/**
 * @brief Deserializes OpenPBR material data from JSON string.
 * 
 * @param this_ Pointer to OpenPBRImpl instance.
 * @param json JSON string containing material data.
 */
void OpenPBRInterface::load_from_json(void *this_, luisa::string_view json) {
    auto *impl = static_cast<OpenPBRImpl *>(this_);
    JsonReader reader(json);
    auto load_texture = [&](vstd::Guid guid) -> RC<world::TextureResource> {
        auto obj = world::get_object_ref(guid);
        if (!obj || !obj->is_type_of<world::TextureResource>()) {
            return {};
        } else {
            return std::move(obj).cast_static<world::TextureResource>();
        }
    };
    impl->maps.clear();
    rbc::detail::serde_openpbr(
        rbc::material::OpenPBR{},
        [&]<typename T>(T const &t, luisa::string_view name) {
            if constexpr (std::is_same_v<MatImageHandle, T>) {
                // Read texture GUID from JSON and load texture
                luisa::string guid_str;
                if (reader.read(guid_str, name.data())) {
                    auto parsed = vstd::Guid::TryParseGuid(guid_str);
                    if (parsed) {
                        auto tex = load_texture(*parsed);
                        if (tex) {
                            impl->maps.try_emplace(vstd::string(name), std::move(tex));
                        }
                    }
                }
            } else {
                // Read value from JSON and emplace to maps
                if constexpr (std::is_same_v<float, T>) {
                    double v;
                    if (reader.read(v, name.data())) {
                        impl->maps.try_emplace(vstd::string(name), static_cast<float>(v));
                    }
                } else if constexpr (std::is_same_v<int, T>) {
                    int64_t v;
                    if (reader.read(v, name.data())) {
                        impl->maps.try_emplace(vstd::string(name), static_cast<int>(v));
                    }
                } else if constexpr (std::is_same_v<uint32_t, T>) {
                    uint64_t v;
                    if (reader.read(v, name.data())) {
                        impl->maps.try_emplace(vstd::string(name), static_cast<uint32_t>(v));
                    }
                } else if constexpr (std::is_same_v<bool, T>) {
                    bool v;
                    if (reader.read(v, name.data())) {
                        impl->maps.try_emplace(vstd::string(name), v);
                    }
                } else if constexpr (luisa::is_vector_v<T>) {
                    constexpr auto dim = luisa::vector_dimension_v<T>;
                    uint64_t size;
                    if (reader.start_array(size, name.data()) && size == dim) {
                        T result;
                        for (auto i : vstd::range(dim)) {
                            if constexpr (std::is_floating_point_v<luisa::vector_element_t<T>>) {
                                double r;
                                if (!reader.read(r)) [[unlikely]]
                                    break;
                                result[i] = r;
                            } else {
                                int64_t r;
                                if (!reader.read(r)) [[unlikely]]
                                    break;
                                result[i] = r;
                            }
                        }
                        reader.end_scope();
                        impl->maps.try_emplace(vstd::string(name), result);
                    }
                }
            }
        });
}

#undef RBC_IMPL_GET_FLOAT
#undef RBC_IMPL_GET_INT
#undef RBC_IMPL_GET_UINT
#undef RBC_IMPL_GET_BOOL
#undef RBC_IMPL_GET_FLOAT2
#undef RBC_IMPL_GET_FLOAT3
#undef RBC_IMPL_GET_TEX
#undef RBC_IMPL_SET_VALUE
#undef RBC_IMPL_SET_TEX

}// namespace rbc
