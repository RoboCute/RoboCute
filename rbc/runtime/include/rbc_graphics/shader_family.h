#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>

#include <luisa/core/stl/filesystem.h>
#include <luisa/core/stl/string.h>
#include <luisa/runtime/shader.h>

#include <rbc_config.h>
#include <rbc_graphics/shader_features.h>
#include <rbc_graphics/shader_manager.h>

namespace rbc {

// A ShaderFamily owns one atomically published set of compatible shader
// programs. Family membership and scene-feature selection rules come from the
// generated shader manifest; consumers only provide the generated ABI loader.
class RBC_RUNTIME_API ShaderFamily {
public:
    using ProgramLoader = ShaderBase const *(*)(
        luisa::filesystem::path const &artifact);
    struct Program {
        luisa::string logical_name;
        ShaderBase const **slot{};
        ProgramLoader load{};
    };
    using Selector = bool (*)(
        luisa::string_view family_name,
        uint64_t scene_feature_mask,
        ShaderManager::VariantSelection &selection);
    using Resolver = bool (*)(
        luisa::string_view family_name,
        ShaderManager::VariantSelection const &selection,
        ShaderManager::VariantFamilyResolution &resolution);

    struct Status {
        bool ready{};
        uint64_t revision{};

        [[nodiscard]] explicit operator bool() const noexcept {
            return ready;
        }
    };

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

public:
    ShaderFamily(
        luisa::string_view family_name,
        std::initializer_list<Program> programs,
        Selector selector = select_with_shader_manager,
        Resolver resolver = resolve_with_shader_manager);
    ~ShaderFamily();

    ShaderFamily(ShaderFamily const &) = delete;
    ShaderFamily(ShaderFamily &&) = delete;
    ShaderFamily &operator=(ShaderFamily const &) = delete;
    ShaderFamily &operator=(ShaderFamily &&) = delete;

    // prefetch/acquire publish bound slots and must be called by one render
    // thread. Background workers only populate private entries.
    void prefetch(SceneShaderFeatureSnapshot features);
    [[nodiscard]] Status acquire(SceneShaderFeatureSnapshot features);

    // Only teardown should wait for dynamic family loads. Render-frame setup
    // intentionally never calls this so a first-time variant cannot stall it.
    void wait();

    [[nodiscard]] static bool select_with_shader_manager(
        luisa::string_view family_name,
        uint64_t scene_feature_mask,
        ShaderManager::VariantSelection &selection);
    [[nodiscard]] static bool resolve_with_shader_manager(
        luisa::string_view family_name,
        ShaderManager::VariantSelection const &selection,
        ShaderManager::VariantFamilyResolution &resolution);
};

}// namespace rbc
