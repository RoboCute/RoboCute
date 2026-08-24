#pragma once
#include "variant_util.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rbc_shader {

constexpr int SOURCE_SCHEMA_VERSION = 2;
constexpr int MAX_SCENE_FEATURES_PER_FAMILY = 16;

using Define = std::pair<std::string, std::optional<std::string>>;

struct SceneFeatures {
    std::vector<std::string> required;
    std::vector<std::string> forbidden;
};

struct DimensionValue {
    std::vector<Define> defines;
    SceneFeatures scene_features;
};

struct Dimension {
    std::string default_value;
    std::map<std::string, DimensionValue> values;
};

struct Permutation {
    std::string identifier;
    std::map<std::string, std::string> selection;
};

struct VariantSet {
    std::string default_permutation;
    std::vector<Permutation> permutations;
};

struct VariantRule {
    std::map<std::string, std::string> selection;
    std::vector<std::string> required_features;
    std::vector<std::string> forbidden_features;

    [[nodiscard]] bool matches(std::vector<std::string> const &enabled) const;
};

struct Program {
    std::string identifier;
    std::string source;
    std::string source_identifier;
    std::string variant_set;
    std::string host_abi; // "stable" or "per_variant"
    std::vector<Define> defines;

    [[nodiscard]] bool uses_bulk_compilation() const {
        return identifier == source_identifier && defines.empty();
    }
};

struct ShaderVariantConfig {
    std::filesystem::path manifest_path;
    std::filesystem::path shader_root;
    std::string source_root;
    std::vector<std::string> include_dirs;
    std::string optimization;
    std::vector<Define> common_defines;
    std::vector<std::string> backends;
    std::vector<std::string> scene_features;
    std::map<std::string, Dimension> dimensions;
    std::map<std::string, VariantSet> variant_sets;
    std::vector<Program> programs;

    [[nodiscard]] std::filesystem::path source_dir() const {
        return shader_root / source_root;
    }
    [[nodiscard]] std::map<std::string, Program> program_map() const;
    [[nodiscard]] std::vector<Program> family_programs(std::string const &variant_set) const;
    [[nodiscard]] std::map<std::string, std::string> default_selection() const;
    [[nodiscard]] std::map<std::string, std::string> effective_selection(
        std::map<std::string, std::string> const &selection) const;
    [[nodiscard]] std::vector<Define> effective_defines(
        std::map<std::string, std::string> const &selection) const;
    [[nodiscard]] std::vector<Define> effective_program_defines(
        Program const &program, std::map<std::string, std::string> const &selection) const;
    [[nodiscard]] VariantRule variant_rule(std::string const &variant_set,
                                           Permutation const &permutation) const;
    [[nodiscard]] std::vector<VariantRule> variant_rules(std::string const &variant_set) const;
    // logical (posix, no extension) -> physical path, sorted by logical.
    [[nodiscard]] std::map<std::string, std::filesystem::path> source_files() const;
};

[[nodiscard]] std::string canonical_selection(std::map<std::string, std::string> const &selection);

// Full manifest load + validation (port of load_config).
[[nodiscard]] ShaderVariantConfig load_config(std::filesystem::path const &manifest_path);

// Copy exactly the inputs covered by tree_digest into an immutable snapshot root.
[[nodiscard]] ShaderVariantConfig snapshot_config(ShaderVariantConfig const &config,
                                                  std::filesystem::path const &snapshot_root);

// Dependency digest over source_root, include dirs and the manifest.
[[nodiscard]] std::string tree_digest(ShaderVariantConfig const &config);

} // namespace rbc_shader
