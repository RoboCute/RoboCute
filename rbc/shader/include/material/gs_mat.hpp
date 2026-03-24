#pragma once
#ifdef __SHADER_LANG__
#include <material/mat_codes.hpp>
#include <virtual_tex/stream.hpp>
#else
#endif
#include <utils/shader_host.hpp>

namespace material {

struct OpenPBRParticle {
    struct Weight {
        float base{1.0f};
        float diffuse_roughness{0.0f};
        float specular{1.0f};
        float metallic{0.0f};
        float transmission{0.0f};
        float coat{0.0f};
    } weight;

    struct Specular {
        std::array<float, 3> specular_color{1.0f, 1.0f, 1.0f};
        float roughness{0.3f};
        float roughness_anisotropy{0.0f};
        float roughness_anisotropy_angle{0.0f};// radians
        float ior{1.5f};
    } specular;

    struct Emission {
        std::array<float, 3> luminance{0.0f, 0.0f, 0.0f};
    } emission;

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
    
    SHADER_CODE(
        static bool transform_to_params(
            BindlessBuffer &buffer_heap,
            BindlessImage &image_heap,
            uint mat_type,
            uint mat_index,
            auto &params,
            uint texture_filter,
            vt::VTMeta vt_meta,
            float2 uv,
            float4 ddxy,
            float3 input_dir,
            bool &reject,
            float3 world_pos,
            auto &&...);)
};

}// namespace material
