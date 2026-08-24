#pragma once
#include "variant_config.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rbc_shader {

constexpr int RUNTIME_SCHEMA_VERSION = 1;
constexpr char const *RUNTIME_MANIFEST_NAME = "shader_manifest.json";
constexpr char const *HOST_INPUT_ID_MARKER = ".shader_input_id";
constexpr char const *RENDER_PLUGIN_INPUT_ID_MARKER = "rbc_render_plugin.input_id";

// Strict runtime-manifest verification (port of verify_shader_root).
// Returns the parsed manifest.
[[nodiscard]] Json verify_shader_root(std::filesystem::path const &shader_root,
                                      std::optional<std::string> const &expected_backend = std::nullopt);

// Verify every backend root and require matching input_id (port of verify_shader_backends).
[[nodiscard]] std::map<std::string, Json> verify_shader_backends(
    std::filesystem::path const &build_root, std::vector<std::string> const &backends);

// Read a .input_id marker and validate it is a lowercase 64-hex digest.
[[nodiscard]] std::string read_input_id_marker(std::filesystem::path const &path, std::string const &context);

// Require backend, hostgen and optional render-plugin generations to match.
[[nodiscard]] std::string verify_shader_coherence(
    std::filesystem::path const &build_root,
    std::vector<std::string> const &backends,
    std::filesystem::path const &host_output,
    std::optional<std::filesystem::path> const &plugin_marker = std::nullopt);

} // namespace rbc_shader
