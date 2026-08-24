#pragma once
#include "variant_config.h"
#include "variant_util.h"
#include "variant_verify.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rbc_shader {

struct VariantArgs {
    std::string command; // validate | build | verify | verify-coherence | lsp | backends
    std::filesystem::path project_root;
    std::filesystem::path manifest; // absolute
    std::filesystem::path compiler; // absolute
    std::filesystem::path build_root;
    std::filesystem::path cache_root;
    std::filesystem::path host_out;
    std::filesystem::path shader_root;
    std::optional<std::filesystem::path> plugin_marker;
    std::vector<std::string> backends;
    std::filesystem::path out; // lsp output
    bool hostgen = false;
    bool hostgen_only = false;
    bool rebuild = false;
    bool quiet = false;
};

// Entry point for --variant=... dispatch. Returns process exit code.
[[nodiscard]] int run_variant_command(VariantArgs const &args);

// Run one compiler invocation (reproc, port of _run_compiler).
void run_compiler(std::vector<std::string> const &command, std::filesystem::path const &cwd);

// Build the child rbcxx command line (port of _compiler_command).
[[nodiscard]] std::vector<std::string> compiler_command(
    ShaderVariantConfig const &config,
    std::filesystem::path const &compiler_path,
    std::filesystem::path const &input_path,
    std::filesystem::path const &output_path,
    std::vector<Define> const &defines,
    std::optional<std::string> const &backend = std::nullopt,
    std::optional<std::filesystem::path> const &cache_dir = std::nullopt,
    std::optional<std::filesystem::path> const &hostgen_path = std::nullopt,
    bool rebuild = false,
    bool lsp = false);

// Build a backend root (port of build_backend).
[[nodiscard]] std::filesystem::path build_backend(
    ShaderVariantConfig const &config,
    std::string const &backend,
    std::filesystem::path const &build_root,
    std::filesystem::path const &cache_root,
    std::filesystem::path const &compiler_path,
    bool rebuild = false);

// Generate host interfaces (port of build_hostgen).
[[nodiscard]] std::filesystem::path build_hostgen(
    ShaderVariantConfig const &config,
    std::filesystem::path const &host_output,
    std::filesystem::path const &cache_root,
    std::filesystem::path const &compiler_path,
    bool rebuild = false);

// Generate compile_commands.json (port of generate_lsp).
[[nodiscard]] std::filesystem::path generate_lsp(
    ShaderVariantConfig const &config,
    std::filesystem::path const &output,
    std::filesystem::path const &compiler_path);

} // namespace rbc_shader
