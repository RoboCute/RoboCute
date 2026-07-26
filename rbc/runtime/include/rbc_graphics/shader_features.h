#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rbc {

enum class SceneShaderFeature : uint64_t {
    ComplexPbrMaterial = 1ull << 0u,
    FreeSpaceDiffraction = 1ull << 1u,
};

[[nodiscard]] constexpr uint64_t scene_shader_feature_mask(SceneShaderFeature feature) noexcept {
    return static_cast<uint64_t>(feature);
}

[[nodiscard]] constexpr std::optional<uint64_t> scene_shader_feature_mask(
    std::string_view feature_name) noexcept {
    if (feature_name == "complex_pbr_material") {
        return scene_shader_feature_mask(SceneShaderFeature::ComplexPbrMaterial);
    }
    if (feature_name == "free_space_diffraction") {
        return scene_shader_feature_mask(SceneShaderFeature::FreeSpaceDiffraction);
    }
    return std::nullopt;
}

struct SceneShaderFeatureSnapshot {
    uint64_t mask{};

    [[nodiscard]] constexpr bool contains(SceneShaderFeature feature) const noexcept {
        return (mask & scene_shader_feature_mask(feature)) != 0u;
    }
};

// OpenPBRParticle intentionally shares this structural check with OpenPBR.
// Texture handles are omitted: textures only modulate an already-enabled lobe.
template<typename Material>
[[nodiscard]] constexpr uint64_t scene_shader_feature_mask(Material const &material) noexcept {
    if constexpr (requires {
                      material.weight.diffuse_roughness;
                      material.weight.metallic;
                      material.weight.transmission;
                      material.weight.coat;
                  }) {
        auto const &weight = material.weight;
        auto lite_compatible =
            weight.diffuse_roughness == 0.0f &&
            weight.coat == 0.0f &&
            (weight.transmission == 0.0f ||
             (weight.transmission == 1.0f && weight.metallic == 0.0f));
        if constexpr (requires { weight.subsurface; }) {
            lite_compatible = lite_compatible && weight.subsurface == 0.0f;
        }
        if constexpr (requires { weight.thin_film; }) {
            lite_compatible = lite_compatible && weight.thin_film == 0.0f;
        }
        if constexpr (requires { weight.fuzz; }) {
            lite_compatible = lite_compatible && weight.fuzz == 0.0f;
        }
        if constexpr (requires { weight.diffraction; }) {
            lite_compatible = lite_compatible && weight.diffraction == 0.0f;
        }
        if constexpr (requires { weight.free_space_diffraction; }) {
            if (weight.free_space_diffraction > 0.0f) {
                return scene_shader_feature_mask(
                           SceneShaderFeature::FreeSpaceDiffraction) |
                       scene_shader_feature_mask(
                           SceneShaderFeature::ComplexPbrMaterial);
            }
        }
        return lite_compatible
                   ? 0u
                   : scene_shader_feature_mask(
                         SceneShaderFeature::ComplexPbrMaterial);
    } else {
        return 0u;
    }
}

namespace detail {
struct ShaderFeatureWeightProbe {
    float diffuse_roughness{};
    float metallic{};
    float transmission{};
    float coat{};
    float subsurface{};
    float thin_film{};
    float fuzz{};
    float diffraction{};
    float free_space_diffraction{};
};
struct ShaderFeatureMaterialProbe {
    ShaderFeatureWeightProbe weight;
};
struct ShaderFeatureParticleWeightProbe {
    float diffuse_roughness{};
    float metallic{};
    float transmission{};
    float coat{};
};
struct ShaderFeatureParticleProbe {
    ShaderFeatureParticleWeightProbe weight;
};

constexpr auto complex_pbr_mask = scene_shader_feature_mask(SceneShaderFeature::ComplexPbrMaterial);
constexpr auto free_space_diffraction_mask =
    scene_shader_feature_mask(SceneShaderFeature::FreeSpaceDiffraction);
static_assert(scene_shader_feature_mask(
                  std::string_view{"free_space_diffraction"}) ==
              std::optional<uint64_t>{free_space_diffraction_mask});
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{}) == 0u);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.transmission = 1.0f}}) == 0u);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.transmission = 0.5f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.metallic = 1.0f, .transmission = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.diffuse_roughness = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.coat = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.subsurface = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.thin_film = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.fuzz = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.diffraction = 1.0f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureMaterialProbe{.weight = {.free_space_diffraction = 1.0f}}) ==
              (free_space_diffraction_mask | complex_pbr_mask));
static_assert(scene_shader_feature_mask(ShaderFeatureParticleProbe{}) == 0u);
static_assert(scene_shader_feature_mask(ShaderFeatureParticleProbe{.weight = {.transmission = 0.5f}}) == complex_pbr_mask);
static_assert(scene_shader_feature_mask(ShaderFeatureParticleProbe{.weight = {.coat = 1.0f}}) == complex_pbr_mask);
}// namespace detail

}// namespace rbc
