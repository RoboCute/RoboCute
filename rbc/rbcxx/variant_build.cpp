#include "variant_build.h"
#include "sha256.h"

#include <reproc++/reproc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

namespace rbc_shader {

namespace {

// Run fn(0..count-1) on a small thread pool. Exceptions from workers are
// captured and rethrown on the calling thread after all workers join.
void parallel_for(std::size_t count, std::function<void(std::size_t)> const &fn) {
    if (count == 0) {
        return;
    }
    auto hw = std::thread::hardware_concurrency();
    std::size_t workers = hw == 0 ? 4 : hw;
    workers = std::min(workers, count);
    std::atomic<std::size_t> next{0};
    std::mutex error_mutex;
    std::exception_ptr first_error;
    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (std::size_t w = 0; w < workers; ++w) {
        threads.emplace_back([&]() {
            while (first_error == nullptr) {
                auto i = next.fetch_add(1);
                if (i >= count) {
                    break;
                }
                try {
                    fn(i);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (first_error == nullptr) {
                        first_error = std::current_exception();
                    }
                    return;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    if (first_error != nullptr) {
        std::rethrow_exception(first_error);
    }
}

constexpr int CACHE_SCHEMA_VERSION = 2;

// Internal exception used by the two-attempt input-stability loop.
class ShaderInputsChanged : public std::exception {
public:
    char const *what() const noexcept override { return "shader inputs changed"; }
};

// RAII cleanup for temporary snapshot/work directories.
class TempDirGuard {
    std::filesystem::path _path;

public:
    explicit TempDirGuard(std::filesystem::path path) : _path(std::move(path)) {}
    ~TempDirGuard() {
        if (!_path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(_path, ec);
        }
    }
    TempDirGuard(TempDirGuard const &) = delete;
    TempDirGuard &operator=(TempDirGuard const &) = delete;
};

// ---------------------------------------------------------------------------
// Json helpers
// ---------------------------------------------------------------------------
Json define_to_json(Define const &define) {
    if (define.second.has_value()) {
        return Json::make_array({Json::make_string(define.first), Json::make_string(*define.second)});
    }
    return Json::make_array({Json::make_string(define.first), Json::null()});
}

Json defines_to_json(std::vector<Define> const &defines) {
    std::vector<Json> items;
    items.reserve(defines.size());
    for (auto const &define : defines) {
        items.push_back(define_to_json(define));
    }
    return Json::make_array(std::move(items));
}

Json selection_to_json(std::map<std::string, std::string> const &selection) {
    std::vector<std::pair<std::string, Json>> entries;
    for (auto const &entry : selection) {
        entries.emplace_back(entry.first, Json::make_string(entry.second));
    }
    return Json::make_object(std::move(entries));
}

Json strings_to_json(std::vector<std::string> const &strings) {
    std::vector<Json> items;
    items.reserve(strings.size());
    for (auto const &value : strings) {
        items.push_back(Json::make_string(value));
    }
    return Json::make_array(std::move(items));
}

std::filesystem::path resolve_path(std::filesystem::path const &path) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return std::filesystem::absolute(path);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Cache validity checks
// ---------------------------------------------------------------------------
bool cache_record_valid(std::filesystem::path const &object_dir, std::string const &compile_key) {
    auto artifact = object_dir / "shader.bin";
    auto metadata_path = object_dir / "metadata.json";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(artifact, ec) ||
        !std::filesystem::is_regular_file(metadata_path, ec)) {
        return false;
    }
    Json metadata;
    try {
        metadata = read_json_file(metadata_path, "cache metadata");
    } catch (std::exception const &) {
        return false;
    }
    if (!metadata.is_object()) {
        return false;
    }
    auto compile_key_value = metadata.get("compile_key");
    auto size_value = metadata.get("size");
    auto sha_value = metadata.get("sha256");
    if (!compile_key_value || !compile_key_value->is_string() || compile_key_value->str != compile_key) {
        return false;
    }
    try {
        auto actual_size = std::filesystem::file_size(artifact, ec);
        if (ec || !size_value || !size_value->is_int() ||
            actual_size != static_cast<std::uintmax_t>(size_value->integer)) {
            return false;
        }
        if (!sha_value || !sha_value->is_string() || sha256_file(artifact) != sha_value->str) {
            return false;
        }
    } catch (std::exception const &) {
        return false;
    }
    return true;
}

bool host_tree_cache_valid(std::filesystem::path const &object_dir,
                           std::string const &hostgen_key,
                           std::vector<std::string> const &required_interfaces) {
    auto tree = object_dir / "tree";
    auto metadata_path = object_dir / "metadata.json";
    std::error_code ec;
    if (!std::filesystem::is_directory(tree, ec) ||
        !std::filesystem::is_regular_file(metadata_path, ec)) {
        return false;
    }
    Json metadata;
    try {
        metadata = read_json_file(metadata_path, "cache metadata");
    } catch (std::exception const &) {
        return false;
    }
    if (!metadata.is_object()) {
        return false;
    }
    auto hostgen_key_value = metadata.get("hostgen_key");
    auto files_value = metadata.get("files");
    if (!hostgen_key_value || !hostgen_key_value->is_string() || hostgen_key_value->str != hostgen_key ||
        !files_value || !files_value->is_object()) {
        return false;
    }
    for (auto const &relative : required_interfaces) {
        if (files_value->get(relative) == nullptr) {
            return false;
        }
    }
    for (auto const &entry : files_value->obj) {
        auto record = entry.second;
        if (record.is_object() == false) {
            return false;
        }
        // safe_relative validation
        auto relative = entry.first;
        if (relative.find('\\') != std::string::npos || relative.find(':') != std::string::npos ||
            relative.empty() || relative.front() == '/') {
            return false;
        }
        auto generated = tree / relative;
        auto size_value = record.get("size");
        auto sha_value = record.get("sha256");
        try {
            if (!std::filesystem::is_regular_file(generated, ec) || !size_value || !size_value->is_int() ||
                std::filesystem::file_size(generated, ec) != static_cast<std::uintmax_t>(size_value->integer) ||
                !sha_value || !sha_value->is_string() || sha256_file(generated) != sha_value->str) {
                return false;
            }
        } catch (std::exception const &) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Materialization helpers (ports of _materialize_default_tree / _compile_variant
// / _materialize_hostgen_tree).
// ---------------------------------------------------------------------------
// Compile one source+defines unit and publish its binary into `destination`.
// The cache key depends only on this unit's own inputs (source, its transitive
// project includes, defines, backend, compiler) so editing an unrelated shader
// never invalidates this unit.
std::string compile_unit(ShaderVariantConfig const &config,
                         std::filesystem::path const &source_path,
                         std::vector<Define> const &defines,
                         std::string const &backend,
                         std::filesystem::path const &compiler_path,
                         CompilerInfo const &compiler_info,
                         std::filesystem::path const &cache_root,
                         std::filesystem::path const &destination,
                         bool rebuild) {
    auto source_relative = path_to_posix(source_path.lexically_relative(config.shader_root));
    Json compile_payload = Json::make_object({
        {"cache_schema", Json::make_int(CACHE_SCHEMA_VERSION)},
        {"compiler", Json::make_string(compiler_info.sha256)},
        {"backend", Json::make_string(backend)},
        {"optimization", Json::make_string(config.optimization)},
        {"source", Json::make_string(source_relative)},
        {"include_dirs", strings_to_json(config.include_dirs)},
        {"defines", defines_to_json(defines)},
        {"unit_digest", Json::make_string(unit_digest(config, source_path))},
    });
    auto compile_key = sha256_hex(canonical_json(compile_payload));
    auto object_dir = cache_root / "objects" / compile_key;
    auto lock_path = cache_root / "locks" / ("variant-" + compile_key + ".lock");
    FileLock lock(lock_path);
    lock.lock();
    if (rebuild || !cache_record_valid(object_dir, compile_key)) {
        auto work_parent = cache_root / "work";
        auto work_dir = make_temp_dir(work_parent, compile_key.substr(0, 12) + ".");
        try {
            auto compiled = work_dir / "shader.bin";
            auto command = compiler_command(config, compiler_path, source_path,
                                            compiled, defines, backend, std::nullopt, std::nullopt, false);
            run_compiler(command, work_dir);
            if (!std::filesystem::is_regular_file(compiled)) {
                throw ShaderVariantError(
                    "Compiler did not produce shader " + source_relative);
            }
            auto publish_parent = cache_root / "publish";
            auto publish_dir = make_temp_dir(publish_parent, compile_key.substr(0, 12) + ".");
            try {
                auto artifact = publish_dir / "shader.bin";
                std::filesystem::copy_file(compiled, artifact, std::filesystem::copy_options::overwrite_existing);
                write_json_atomic(publish_dir / "metadata.json",
                                  Json::make_object({
                                      {"compile_key", Json::make_string(compile_key)},
                                      {"sha256", Json::make_string(sha256_file(artifact))},
                                      {"size", Json::make_int(static_cast<int64_t>(std::filesystem::file_size(artifact)))},
                                  }));
                std::error_code mkdir_ec;
                std::filesystem::create_directories(object_dir.parent_path(), mkdir_ec);
                atomic_replace_directory(publish_dir, object_dir);
            } catch (...) {
                if (std::filesystem::exists(publish_dir)) {
                    remove_path(publish_dir);
                }
                throw;
            }
        } catch (...) {
            if (std::filesystem::exists(work_dir)) {
                remove_path(work_dir);
            }
            throw;
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    copy_if_different(object_dir / "shader.bin", destination);
    return compile_key;
}

// Compile every default (no variant selection) unit in parallel and assemble the
// top-level default tree. Returns logical -> compile_key for each unit so the
// manifest can reference the per-unit cache keys.
std::map<std::string, std::string> materialize_default_tree(
    ShaderVariantConfig const &config,
    std::string const &backend,
    std::filesystem::path const &compiler_path,
    CompilerInfo const &compiler_info,
    std::vector<Define> const &default_defines,
    std::map<std::string, std::filesystem::path> const &source_programs,
    std::filesystem::path const &cache_root,
    std::filesystem::path const &destination,
    bool rebuild) {
    std::vector<std::string> logicals;
    logicals.reserve(source_programs.size());
    for (auto const &entry : source_programs) {
        logicals.push_back(entry.first);
    }
    std::vector<std::string> keys(logicals.size());
    parallel_for(logicals.size(), [&](std::size_t index) {
        auto const &logical = logicals[index];
        auto const &source = source_programs.at(logical);
        keys[index] = compile_unit(config, source, default_defines, backend, compiler_path,
                                   compiler_info, cache_root, destination / (logical + ".bin"), rebuild);
    });
    std::map<std::string, std::string> result;
    for (std::size_t i = 0; i < logicals.size(); ++i) {
        result[logicals[i]] = keys[i];
    }
    return result;
}

void compile_program_host_interface(ShaderVariantConfig const &config,
                                    Program const &program,
                                    std::map<std::string, std::string> const &selection,
                                    std::filesystem::path const &compiler_path,
                                    std::filesystem::path const &output_path,
                                    std::filesystem::path const &work_dir,
                                    std::filesystem::path const &cache_dir,
                                    bool rebuild) {
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (std::filesystem::exists(output_path)) {
        std::filesystem::remove(output_path, ec);
    }
    auto binary_path = work_dir / "program-out" / (program.identifier + ".bin");
    std::filesystem::create_directories(binary_path.parent_path(), ec);
    auto command = compiler_command(config, compiler_path, config.shader_root / program.source,
                                    binary_path, config.effective_program_defines(program, selection),
                                    std::nullopt, cache_dir, output_path, rebuild);
    run_compiler(command, work_dir);
}

void materialize_hostgen_tree(ShaderVariantConfig const &config,
                              std::filesystem::path const &compiler_path,
                              std::string const &hostgen_key,
                              std::vector<Define> const &default_defines,
                              std::map<std::string, std::string> const &selection,
                              std::vector<Program> const &required_programs,
                              std::filesystem::path const &cache_root,
                              std::filesystem::path const &destination,
                              bool rebuild) {
    std::vector<std::string> required_interfaces;
    required_interfaces.reserve(required_programs.size());
    for (auto const &program : required_programs) {
        required_interfaces.push_back(program.identifier + ".inl");
    }
    auto object_dir = cache_root / "host_objects" / hostgen_key;
    auto lock_path = cache_root / "locks" / ("host-object-" + hostgen_key + ".lock");
    FileLock lock(lock_path);
    lock.lock();
    if (rebuild || !host_tree_cache_valid(object_dir, hostgen_key, required_interfaces)) {
        auto compiler_cache = destination.parent_path() / ("compiler-cache-" + hostgen_key.substr(0, 16));
        std::error_code ec;
        std::filesystem::create_directories(compiler_cache, ec);
        auto command = compiler_command(config, compiler_path, config.source_dir(),
                                        destination.parent_path() / "out", default_defines,
                                        std::nullopt, compiler_cache, destination, rebuild);
        run_compiler(command, destination.parent_path());
        for (auto const &program : required_programs) {
            if (program.uses_bulk_compilation()) {
                continue;
            }
            compile_program_host_interface(config, program, selection, compiler_path,
                                           destination / (program.identifier + ".inl"),
                                           destination.parent_path(), compiler_cache, rebuild);
        }
        for (auto const &relative : required_interfaces) {
            if (!std::filesystem::is_regular_file(destination / relative)) {
                throw ShaderVariantError(
                    "Hostgen did not produce required interface " + relative + " in this invocation");
            }
        }
        std::vector<std::filesystem::path> generated_files;
        std::error_code iter_ec;
        for (auto const &entry : std::filesystem::recursive_directory_iterator(destination, iter_ec)) {
            if (entry.is_regular_file(iter_ec) && entry.path().extension() == ".inl") {
                generated_files.push_back(entry.path());
            }
        }
        std::sort(generated_files.begin(), generated_files.end(), [](auto const &a, auto const &b) {
            return a.generic_string() < b.generic_string();
        });
        if (generated_files.empty()) {
            throw ShaderVariantError("Hostgen did not produce any host interfaces");
        }
        std::vector<std::pair<std::string, Json>> file_records;
        for (auto const &path : generated_files) {
            auto relative = path_to_posix(path.lexically_relative(destination));
            file_records.emplace_back(relative, Json::make_object({
                                                    {"sha256", Json::make_string(sha256_file(path))},
                                                    {"size", Json::make_int(static_cast<int64_t>(std::filesystem::file_size(path)))},
                                                }));
        }
        auto publish_parent = cache_root / "host_publish";
        auto publish_dir = make_temp_dir(publish_parent, hostgen_key.substr(0, 12) + ".");
        try {
            copy_tree(destination, publish_dir / "tree");
            write_json_atomic(publish_dir / "metadata.json",
                              Json::make_object({
                                  {"hostgen_key", Json::make_string(hostgen_key)},
                                  {"files", Json::make_object(std::move(file_records))},
                              }));
            std::error_code mkdir_ec;
            std::filesystem::create_directories(object_dir.parent_path(), mkdir_ec);
            atomic_replace_directory(publish_dir, object_dir);
        } catch (...) {
            if (std::filesystem::exists(publish_dir)) {
                remove_path(publish_dir);
            }
            throw;
        }
    } else {
        copy_tree_if_different(object_dir / "tree", destination);
    }
}

// ---------------------------------------------------------------------------
// Stable ABI validation
// ---------------------------------------------------------------------------
struct StableAbiSelection {
    std::string label;
    std::map<std::string, std::string> selection;
    std::vector<Define> defines;
};

std::vector<StableAbiSelection> stable_abi_selections(ShaderVariantConfig const &config) {
    std::vector<StableAbiSelection> selections;
    std::set<std::vector<Define>> seen_defines;
    auto default_defines = config.effective_defines({});
    seen_defines.insert(default_defines);
    selections.push_back({"default", {}, default_defines});
    for (auto const &set_entry : config.variant_sets) {
        auto const &set_name = set_entry.first;
        std::vector<Permutation const *> sorted_permutations;
        for (auto const &permutation : set_entry.second.permutations) {
            sorted_permutations.push_back(&permutation);
        }
        std::sort(sorted_permutations.begin(), sorted_permutations.end(),
                  [](Permutation const *a, Permutation const *b) {
                      return canonical_selection(a->selection) < canonical_selection(b->selection);
                  });
        for (auto const *permutation : sorted_permutations) {
            auto defines = config.effective_defines(permutation->selection);
            if (seen_defines.find(defines) != seen_defines.end()) {
                continue;
            }
            seen_defines.insert(defines);
            selections.push_back({set_name + ":" + canonical_selection(permutation->selection),
                                  permutation->selection, defines});
        }
    }
    return selections;
}

void validate_stable_host_abi(ShaderVariantConfig const &config,
                              std::filesystem::path const &cache_root,
                              std::filesystem::path const &compiler_path,
                              CompilerInfo const &compiler_info,
                              std::string const &dependency_digest,
                              bool rebuild) {
    std::vector<Program> stable_programs;
    for (auto const &program : config.programs) {
        if (program.host_abi == "stable") {
            stable_programs.push_back(program);
        }
    }
    if (stable_programs.empty()) {
        return;
    }
    auto selections = stable_abi_selections(config);
    std::vector<Json> define_sets;
    for (auto const &selection : selections) {
        define_sets.push_back(defines_to_json(selection.defines));
    }
    std::vector<Json> program_define_sets;
    for (auto const &selection : selections) {
        std::vector<std::pair<std::string, Json>> entries;
        for (auto const &program : stable_programs) {
            entries.emplace_back(program.identifier,
                                 defines_to_json(config.effective_program_defines(program, selection.selection)));
        }
        program_define_sets.push_back(Json::make_object(std::move(entries)));
    }
    std::vector<Json> program_ids;
    for (auto const &program : stable_programs) {
        program_ids.push_back(Json::make_string(program.identifier));
    }
    Json validation_payload = Json::make_object({
        {"cache_schema", Json::make_int(CACHE_SCHEMA_VERSION)},
        {"compiler", Json::make_string(compiler_info.sha256)},
        {"dependency_digest", Json::make_string(dependency_digest)},
        {"programs", Json::make_array(std::move(program_ids))},
        {"define_sets", Json::make_array(std::move(define_sets))},
        {"program_define_sets", Json::make_array(std::move(program_define_sets))},
    });
    auto validation_key = sha256_hex(canonical_json(validation_payload));
    auto validation_path = cache_root / "host_abi" / (validation_key + ".json");
    std::set<std::string> expected_programs;
    for (auto const &program : stable_programs) {
        expected_programs.insert(program.identifier);
    }
    std::vector<std::string> expected_define_sets;
    for (auto const &selection : selections) {
        expected_define_sets.push_back(selection.label);
    }

    auto cache_valid = [&]() -> bool {
        if (rebuild || !std::filesystem::is_regular_file(validation_path)) {
            return false;
        }
        Json cached;
        try {
            cached = read_json_file(validation_path, "stable ABI validation");
        } catch (std::exception const &) {
            return false;
        }
        if (!cached.is_object()) {
            return false;
        }
        auto key_value = cached.get("validation_key");
        auto programs_value = cached.get("programs");
        auto define_sets_value = cached.get("define_sets");
        if (!key_value || !key_value->is_string() || key_value->str != validation_key ||
            !programs_value || !programs_value->is_object() || !define_sets_value || !define_sets_value->is_array()) {
            return false;
        }
        std::set<std::string> cached_programs;
        for (auto const &entry : programs_value->obj) {
            cached_programs.insert(entry.first);
        }
        if (cached_programs != expected_programs) {
            return false;
        }
        if (define_sets_value->arr.size() != expected_define_sets.size()) {
            return false;
        }
        for (std::size_t i = 0; i < define_sets_value->arr.size(); ++i) {
            if (!define_sets_value->arr[i].is_string() ||
                define_sets_value->arr[i].str != expected_define_sets[i]) {
                return false;
            }
        }
        return true;
    };

    auto lock_path = cache_root / "locks" / ("host-abi-" + validation_key + ".lock");
    FileLock lock(lock_path);
    lock.lock();
    if (cache_valid()) {
        return;
    }
    auto work_parent = cache_root / "host_abi_work";
    auto work_dir = make_temp_dir(work_parent, validation_key.substr(0, 12) + ".");
    std::optional<std::vector<std::string>> reference;
    std::string reference_label;
    std::vector<std::pair<std::string, Json>> interface_hashes;
    try {
        for (std::size_t index = 0; index < selections.size(); ++index) {
            auto const &selection = selections[index];
            auto selection_root = work_dir / std::to_string(index);
            auto host_dir = selection_root / "host";
            std::error_code ec;
            std::filesystem::create_directories(host_dir, ec);
            auto define_key = sha256_hex(canonical_json(defines_to_json(selection.defines))).substr(0, 16);
            auto compiler_cache = selection_root / ("compiler-cache-" + define_key);
            std::filesystem::create_directories(compiler_cache, ec);
            auto command = compiler_command(config, compiler_path, config.source_dir(),
                                            selection_root / "out", selection.defines,
                                            std::nullopt, compiler_cache, host_dir, rebuild);
            run_compiler(command, selection_root);
            for (auto const &program : stable_programs) {
                if (program.uses_bulk_compilation()) {
                    continue;
                }
                compile_program_host_interface(config, program, selection.selection, compiler_path,
                                               host_dir / (program.identifier + ".inl"),
                                               selection_root, compiler_cache, rebuild);
            }
            std::vector<std::string> current;
            for (auto const &program : stable_programs) {
                auto generated = host_dir / (program.identifier + ".inl");
                if (!std::filesystem::is_regular_file(generated)) {
                    throw ShaderVariantError(
                        "Hostgen did not produce " + program.identifier + ".inl for " + selection.label);
                }
                current.push_back(normalize_host_interface(read_file_text(generated), config.shader_root));
            }
            if (!reference.has_value()) {
                reference = current;
                reference_label = selection.label;
                interface_hashes.clear();
                for (std::size_t i = 0; i < stable_programs.size(); ++i) {
                    interface_hashes.emplace_back(stable_programs[i].identifier,
                                                  Json::make_string(sha256_hex(current[i])));
                }
                continue;
            }
            for (std::size_t i = 0; i < stable_programs.size(); ++i) {
                if (current[i] != (*reference)[i]) {
                    throw ShaderVariantError("Stable host ABI changed for " + stable_programs[i].identifier +
                                             ": " + reference_label + " != " + selection.label);
                }
            }
        }
        std::vector<Json> define_sets_labels;
        for (auto const &label : expected_define_sets) {
            define_sets_labels.push_back(Json::make_string(label));
        }
        write_json_atomic(validation_path,
                          Json::make_object({
                              {"validation_key", Json::make_string(validation_key)},
                              {"programs", Json::make_object(std::move(interface_hashes))},
                              {"define_sets", Json::make_array(std::move(define_sets_labels))},
                          }));
    } catch (...) {
        if (std::filesystem::exists(work_dir)) {
            remove_path(work_dir);
        }
        throw;
    }
    if (std::filesystem::exists(work_dir)) {
        remove_path(work_dir);
    }
}

// ---------------------------------------------------------------------------
// Artifact records and runtime manifest helpers
// ---------------------------------------------------------------------------
Json artifact_record(std::filesystem::path const &root,
                     std::map<std::string, std::string> const &selection,
                     std::string const &artifact,
                     std::optional<std::string> const &compile_key) {
    auto artifact_path = root / artifact;
    if (!std::filesystem::is_regular_file(artifact_path)) {
        throw ShaderVariantError("Missing shader artifact: " + artifact);
    }
    std::vector<std::pair<std::string, Json>> entries;
    entries.emplace_back("selection", selection_to_json(selection));
    entries.emplace_back("artifact", Json::make_string(artifact));
    entries.emplace_back("sha256", Json::make_string(sha256_file(artifact_path)));
    entries.emplace_back("size", Json::make_int(static_cast<int64_t>(std::filesystem::file_size(artifact_path))));
    if (!selection.empty()) {
        entries.emplace_back("canonical_key", Json::make_string(canonical_selection(selection)));
    }
    if (compile_key.has_value()) {
        entries.emplace_back("compile_key", Json::make_string(*compile_key));
    }
    return Json::make_object(std::move(entries));
}

Json runtime_families(ShaderVariantConfig const &config) {
    std::vector<std::pair<std::string, Json>> families;
    for (auto const &family_entry : config.variant_sets) {
        auto const &family_name = family_entry.first;
        std::vector<Json> program_ids;
        for (auto const &program : config.family_programs(family_name)) {
            program_ids.push_back(Json::make_string(program.identifier));
        }
        std::vector<Json> rules;
        for (auto const &rule : config.variant_rules(family_name)) {
            rules.push_back(Json::make_object({
                {"selection", selection_to_json(rule.selection)},
                {"required_features", strings_to_json(rule.required_features)},
                {"forbidden_features", strings_to_json(rule.forbidden_features)},
            }));
        }
        families.emplace_back(family_name, Json::make_object({
                                               {"programs", Json::make_array(std::move(program_ids))},
                                               {"rules", Json::make_array(std::move(rules))},
                                           }));
    }
    return Json::make_object(std::move(families));
}

// ---------------------------------------------------------------------------
// Backend build (port of _build_backend_locked / build_backend).
// ---------------------------------------------------------------------------
std::filesystem::path build_backend_locked(ShaderVariantConfig const &config,
                                           std::string const &backend,
                                           std::filesystem::path const &build_root,
                                           std::filesystem::path const &cache_root,
                                           std::filesystem::path const &compiler_path,
                                           CompilerInfo const &compiler_info,
                                           std::string const &dependency_digest,
                                           std::filesystem::path const &destination,
                                           std::function<void()> const &validate_before_publish,
                                           bool rebuild) {
    recover_directory_backup(destination);
    auto staged = make_temp_dir(build_root, ".shader_build_" + backend + ".");
    bool published = false;
    try {
        auto default_defines = config.effective_defines({});
        auto source_programs = config.source_files();
        std::map<std::string, std::string> default_unit_keys;
        default_unit_keys = materialize_default_tree(
            config, backend, compiler_path, compiler_info, default_defines, source_programs,
            cache_root, staged, rebuild);

        auto declared_programs = config.program_map();
        std::map<std::string, std::filesystem::path> logical_sources = source_programs;
        for (auto const &program : config.programs) {
            logical_sources[program.identifier] = config.shader_root / program.source;
        }

        // Collect every variant compile task. Tasks are independent (distinct
        // destination paths and per-key cache locks) so they run in parallel.
        struct VariantTask {
            std::string logical;
            Program const *program;
            Permutation const *permutation;
            std::string artifact;
            std::string selection_key;
        };
        std::vector<VariantTask> tasks;
        std::map<std::string, std::map<std::string, std::size_t>> task_index;
        for (auto const &logical_entry : logical_sources) {
            auto const &logical = logical_entry.first;
            auto declared_iter = declared_programs.find(logical);
            if (declared_iter == declared_programs.end()) {
                continue;
            }
            auto const &declared = declared_iter->second;
            auto const &variant_set = config.variant_sets.at(declared.variant_set);
            Permutation const *default_permutation = nullptr;
            for (auto const &permutation : variant_set.permutations) {
                if (permutation.identifier == variant_set.default_permutation) {
                    default_permutation = &permutation;
                    break;
                }
            }
            if (default_permutation == nullptr) {
                throw ShaderVariantError("Default permutation " + variant_set.default_permutation +
                                         " is missing from variant set " + declared.variant_set);
            }
            if (!declared.uses_bulk_compilation()) {
                auto canonical = canonical_selection(default_permutation->selection);
                task_index[logical][canonical] = tasks.size();
                tasks.push_back({logical, &declared, default_permutation, logical + ".bin", canonical});
            }
            std::vector<Permutation const *> non_default;
            for (auto const &permutation : variant_set.permutations) {
                if (permutation.identifier != variant_set.default_permutation) {
                    non_default.push_back(&permutation);
                }
            }
            std::sort(non_default.begin(), non_default.end(),
                      [](Permutation const *a, Permutation const *b) {
                          return canonical_selection(a->selection) < canonical_selection(b->selection);
                      });
            for (auto const *permutation : non_default) {
                auto canonical = canonical_selection(permutation->selection);
                auto artifact = "variants/" + canonical + "/" + logical + ".bin";
                task_index[logical][canonical] = tasks.size();
                tasks.push_back({logical, &declared, permutation, artifact, canonical});
            }
        }
        std::vector<std::string> task_keys(tasks.size());
        parallel_for(tasks.size(), [&](std::size_t index) {
            auto const &task = tasks[index];
            auto defines = config.effective_program_defines(*task.program, task.permutation->selection);
            task_keys[index] = compile_unit(
                config, config.shader_root / task.program->source, defines, backend, compiler_path,
                compiler_info, cache_root, staged / task.artifact, rebuild);
        });

        // Assemble runtime manifest data from the finished tasks.
        std::vector<std::pair<std::string, Json>> runtime_program_entries;
        std::vector<Json> build_inputs;
        for (auto const &logical_entry : logical_sources) {
            auto const &logical = logical_entry.first;
            auto const &source = logical_entry.second;
            auto default_artifact = logical + ".bin";
            auto declared_iter = declared_programs.find(logical);
            std::map<std::string, std::string> default_selection;
            std::vector<Json> variants;
            if (declared_iter == declared_programs.end()) {
                default_selection = {};
                variants.push_back(artifact_record(staged, default_selection, default_artifact,
                                                   default_unit_keys.at(logical)));
            } else {
                auto const &declared = declared_iter->second;
                auto const &variant_set = config.variant_sets.at(declared.variant_set);
                Permutation const *default_permutation = nullptr;
                for (auto const &permutation : variant_set.permutations) {
                    if (permutation.identifier == variant_set.default_permutation) {
                        default_permutation = &permutation;
                        break;
                    }
                }
                if (default_permutation == nullptr) {
                    throw ShaderVariantError("Default permutation " + variant_set.default_permutation +
                                             " is missing from variant set " + declared.variant_set);
                }
                default_selection = default_permutation->selection;
                std::string default_key;
                if (declared.uses_bulk_compilation()) {
                    default_key = default_unit_keys.at(logical);
                } else {
                    auto canonical = canonical_selection(default_permutation->selection);
                    default_key = task_keys[task_index.at(logical).at(canonical)];
                }
                variants.push_back(artifact_record(staged, default_selection, default_artifact, default_key));
                std::vector<Permutation const *> non_default;
                for (auto const &permutation : variant_set.permutations) {
                    if (permutation.identifier != variant_set.default_permutation) {
                        non_default.push_back(&permutation);
                    }
                }
                std::sort(non_default.begin(), non_default.end(),
                          [](Permutation const *a, Permutation const *b) {
                              return canonical_selection(a->selection) < canonical_selection(b->selection);
                          });
                for (auto const *permutation : non_default) {
                    auto canonical = canonical_selection(permutation->selection);
                    auto artifact = "variants/" + canonical + "/" + logical + ".bin";
                    auto key = task_keys[task_index.at(logical).at(canonical)];
                    variants.push_back(artifact_record(staged, permutation->selection, artifact, key));
                }
            }
            runtime_program_entries.emplace_back(logical, Json::make_object({
                                                              {"default_selection", selection_to_json(default_selection)},
                                                              {"variants", Json::make_array(variants)},
                                                          }));
            std::vector<Json> variant_summaries;
            for (auto const &variant : variants) {
                Json summary = Json::make_object({
                    {"selection", *variant.get("selection")},
                    {"artifact", *variant.get("artifact")},
                    {"compile_key", *variant.get("compile_key")},
                    {"sha256", *variant.get("sha256")},
                    {"size", *variant.get("size")},
                });
                variant_summaries.push_back(std::move(summary));
            }
            build_inputs.push_back(Json::make_object({
                {"program", Json::make_string(logical)},
                {"source", Json::make_string(path_to_posix(source.lexically_relative(config.shader_root)))},
                {"variants", Json::make_array(std::move(variant_summaries))},
            }));
        }

        Json build_id_payload = Json::make_object({
            {"backend", Json::make_string(backend)},
            {"compiler", Json::make_string(compiler_info.sha256)},
            {"dependency_digest", Json::make_string(dependency_digest)},
            {"programs", Json::make_array(std::move(build_inputs))},
            {"source_schema", Json::make_int(SOURCE_SCHEMA_VERSION)},
        });
        std::vector<std::pair<std::string, Json>> compiler_entries;
        for (auto const &file_entry : compiler_info.files) {
            compiler_entries.emplace_back(file_entry.first, Json::make_string(file_entry.second));
        }
        Json runtime_manifest = Json::make_object({
            {"schema_version", Json::make_int(RUNTIME_SCHEMA_VERSION)},
            {"backend", Json::make_string(backend)},
            {"build_id", Json::make_string(sha256_hex(canonical_json(build_id_payload)))},
            {"input_id", Json::make_string(build_input_id(dependency_digest, compiler_info))},
            {"compiler", Json::make_object({
                             {"id", Json::make_string(compiler_info.id)},
                             {"sha256", Json::make_string(compiler_info.sha256)},
                             {"files", Json::make_object(std::move(compiler_entries))},
                         })},
            {"programs", Json::make_object(std::move(runtime_program_entries))},
            {"families", runtime_families(config)},
        });
        write_json_atomic(staged / RUNTIME_MANIFEST_NAME, runtime_manifest);
        verify_shader_root(staged, backend);
        if (validate_before_publish) {
            validate_before_publish();
        }
        atomic_replace_directory(staged, destination);
        published = true;
        return destination;
    } catch (...) {
        if (!published && std::filesystem::exists(staged)) {
            remove_path(staged);
        }
        throw;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------
void run_compiler(std::vector<std::string> const &command, std::filesystem::path const &cwd) {
    std::vector<char const *> argv;
    argv.reserve(command.size() + 1);
    for (auto const &arg : command) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    reproc::options options;
    options.redirect.in.type = reproc::redirect::default_;
    options.redirect.out.type = reproc::redirect::default_;
    options.redirect.err.type = reproc::redirect::default_;
    auto cwd_string = cwd.string();
    options.working_directory = cwd_string.c_str();
    reproc::process process;
    if (auto error = process.start(reproc::arguments{argv.data()}, options)) {
        throw ShaderVariantError("Failed to start compiler: " + error.message());
    }
    auto result = process.wait(1024h);
    if (result.second) {
        throw ShaderVariantError("Compiler process failed: " + result.second.message());
    }
    if (result.first != 0) {
        throw ShaderVariantError("Compiler returned non-zero exit status " + std::to_string(result.first));
    }
}

std::vector<std::string> compiler_command(ShaderVariantConfig const &config,
                                          std::filesystem::path const &compiler_path,
                                          std::filesystem::path const &input_path,
                                          std::filesystem::path const &output_path,
                                          std::vector<Define> const &defines,
                                          std::optional<std::string> const &backend,
                                          std::optional<std::filesystem::path> const &cache_dir,
                                          std::optional<std::filesystem::path> const &hostgen_path,
                                          bool rebuild,
                                          bool lsp) {
    std::vector<std::string> command;
    command.push_back(compiler_path.string());
    command.push_back("--in=" + input_path.string());
    command.push_back("--out=" + output_path.string());
    for (auto const &include_dir : config.include_dirs) {
        command.push_back("--include=" + (config.shader_root / include_dir).string());
    }
    for (auto const &define : defines) {
        if (define.second.has_value()) {
            command.push_back("--D=" + define.first + "=" + *define.second);
        } else {
            command.push_back("--D=" + define.first);
        }
    }
    if (backend.has_value()) {
        command.push_back("--backend=" + *backend);
        command.push_back("--opt=" + config.optimization);
    }
    if (cache_dir.has_value()) {
        command.push_back("--cache_dir=" + cache_dir->string());
    }
    if (hostgen_path.has_value()) {
        command.push_back("--hostgen=" + hostgen_path->string());
    }
    if (rebuild) {
        command.push_back("--rebuild");
    }
    if (lsp) {
        command.push_back("--lsp");
    }
    return command;
}

std::filesystem::path build_backend(ShaderVariantConfig const &config,
                                    std::string const &backend,
                                    std::filesystem::path const &build_root,
                                    std::filesystem::path const &cache_root,
                                    std::filesystem::path const &compiler_path,
                                    bool rebuild) {
    auto build_root_resolved = resolve_path(build_root);
    auto cache_root_resolved = resolve_path(cache_root);
    auto compiler_path_resolved = resolve_path(compiler_path);
    std::error_code ec;
    std::filesystem::create_directories(build_root_resolved, ec);
    std::filesystem::create_directories(cache_root_resolved, ec);
    auto compiler_info = compiler_fingerprint(compiler_path_resolved);
    auto destination = build_root_resolved / ("shader_build_" + backend);
    auto publish_lock = build_root_resolved / ".shader_locks" / ("shader_build_" + backend + ".lock");
    FileLock lock(publish_lock);
    lock.lock();
    auto snapshot_parent = cache_root_resolved / "input_snapshots";
    std::filesystem::create_directories(snapshot_parent, ec);
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto live_config = load_config(config.manifest_path);
        if (std::find(live_config.backends.begin(), live_config.backends.end(), backend) ==
            live_config.backends.end()) {
            throw ShaderVariantError("Backend " + backend + " is not declared by the manifest");
        }
        auto snapshot_root = make_temp_dir(snapshot_parent, backend + ".");
        TempDirGuard snapshot_guard(snapshot_root);
        try {
            auto snapshot_cfg = snapshot_config(live_config, snapshot_root);
            auto dependency_digest = tree_digest(snapshot_cfg);
            validate_stable_host_abi(snapshot_cfg, cache_root_resolved, compiler_path_resolved,
                                     compiler_info, dependency_digest, rebuild);
            std::function<void()> validate_inputs = [&]() {
                auto current_config = load_config(config.manifest_path);
                if (tree_digest(current_config) != dependency_digest) {
                    throw ShaderInputsChanged();
                }
            };
            return build_backend_locked(snapshot_cfg, backend, build_root_resolved, cache_root_resolved,
                                        compiler_path_resolved, compiler_info, dependency_digest,
                                        destination, validate_inputs, rebuild);
        } catch (ShaderInputsChanged const &) {
            if (attempt == 1) {
                throw ShaderVariantError(
                    "Shader inputs changed repeatedly during compilation; the previous shader build was preserved");
            }
        }
    }
    throw ShaderVariantError("unreachable");
}

std::filesystem::path build_hostgen(ShaderVariantConfig const &config,
                                    std::filesystem::path const &host_output,
                                    std::filesystem::path const &cache_root,
                                    std::filesystem::path const &compiler_path,
                                    bool rebuild) {
    auto host_output_resolved = resolve_path(host_output);
    auto cache_root_resolved = resolve_path(cache_root);
    auto compiler_path_resolved = resolve_path(compiler_path);
    std::error_code ec;
    std::filesystem::create_directories(host_output_resolved.parent_path(), ec);
    std::filesystem::create_directories(cache_root_resolved, ec);
    auto compiler_info = compiler_fingerprint(compiler_path_resolved);
    auto output_key = sha256_hex(path_casefold(host_output_resolved));
    auto host_lock = cache_root_resolved / "locks" / ("host-publish-" + output_key + ".lock");
    FileLock lock(host_lock);
    lock.lock();
    recover_directory_backup(host_output_resolved);
    auto snapshot_parent = cache_root_resolved / "input_snapshots";
    std::filesystem::create_directories(snapshot_parent, ec);
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto live_config = load_config(config.manifest_path);
        auto snapshot_root = make_temp_dir(snapshot_parent, "host.");
        TempDirGuard snapshot_guard(snapshot_root);
        try {
            auto snapshot_cfg = snapshot_config(live_config, snapshot_root);
            auto dependency_digest = tree_digest(snapshot_cfg);
            validate_stable_host_abi(snapshot_cfg, cache_root_resolved, compiler_path_resolved,
                                     compiler_info, dependency_digest, rebuild);
            auto default_defines = snapshot_cfg.effective_defines({});
            std::vector<Program> stable_programs;
            for (auto const &program : snapshot_cfg.programs) {
                if (program.host_abi == "stable") {
                    stable_programs.push_back(program);
                }
            }
            std::map<std::string, std::vector<Program>> per_variant_families;
            for (auto const &variant_set_entry : snapshot_cfg.variant_sets) {
                std::vector<Program> programs;
                for (auto const &program : snapshot_cfg.family_programs(variant_set_entry.first)) {
                    if (program.host_abi == "per_variant") {
                        programs.push_back(program);
                    }
                }
                if (!programs.empty()) {
                    per_variant_families[variant_set_entry.first] = std::move(programs);
                }
            }
            std::map<std::string, Program> default_required_map;
            for (auto const &program : stable_programs) {
                default_required_map[program.identifier] = program;
            }
            for (auto const &family_entry : per_variant_families) {
                auto const &variant_set = snapshot_cfg.variant_sets.at(family_entry.first);
                for (auto const &permutation : variant_set.permutations) {
                    auto defines = snapshot_cfg.effective_defines(permutation.selection);
                    if (defines == default_defines) {
                        for (auto const &program : family_entry.second) {
                            default_required_map[program.identifier] = program;
                        }
                    }
                }
            }
            std::vector<Program> default_required_programs;
            for (auto const &entry : default_required_map) {
                default_required_programs.push_back(entry.second);
            }
            std::sort(default_required_programs.begin(), default_required_programs.end(),
                      [](Program const &a, Program const &b) { return a.identifier < b.identifier; });

            std::vector<Json> hostgen_programs;
            for (auto const &program : default_required_programs) {
                hostgen_programs.push_back(Json::make_object({
                    {"id", Json::make_string(program.identifier)},
                    {"source", Json::make_string(program.source)},
                    {"defines", defines_to_json(snapshot_cfg.effective_program_defines(program, {}))},
                }));
            }
            Json hostgen_payload = Json::make_object({
                {"cache_schema", Json::make_int(CACHE_SCHEMA_VERSION)},
                {"compiler", Json::make_string(compiler_info.sha256)},
                {"dependency_digest", Json::make_string(dependency_digest)},
                {"defines", defines_to_json(default_defines)},
                {"programs", Json::make_array(std::move(hostgen_programs))},
            });
            auto hostgen_key = sha256_hex(canonical_json(hostgen_payload));

            auto work_dir = make_temp_dir(host_output_resolved.parent_path(), ".shader_hostgen.");
            TempDirGuard work_guard(work_dir);
            auto generated_host = work_dir / "generated";
            std::filesystem::create_directories(generated_host, ec);
            materialize_hostgen_tree(snapshot_cfg, compiler_path_resolved, hostgen_key, default_defines,
                                         {}, default_required_programs, cache_root_resolved, generated_host,
                                         rebuild);
            // Collect per-variant hostgen tasks; the expensive materialize calls
            // (heavy path-tracer host ABI compiles) run in parallel.
            struct HostVariantTask {
                std::string family_name;
                std::string permutation_id;
                std::string variant_hostgen_key;
                std::vector<Define> defines;
                std::map<std::string, std::string> selection;
                std::vector<Program const *> programs;
                std::vector<std::string> variant_interfaces;
                std::filesystem::path raw_host;
                bool materialize;
            };
            std::vector<HostVariantTask> host_tasks;
            for (auto const &family_entry : per_variant_families) {
                auto const &family_name = family_entry.first;
                auto const &programs = family_entry.second;
                auto const &variant_set = snapshot_cfg.variant_sets.at(family_name);
                for (auto const &permutation : variant_set.permutations) {
                    auto defines = snapshot_cfg.effective_defines(permutation.selection);
                    std::vector<Json> program_ids;
                    for (auto const &program : programs) {
                        program_ids.push_back(Json::make_string(program.identifier));
                    }
                    std::vector<std::pair<std::string, Json>> program_define_entries;
                    for (auto const &program : programs) {
                        program_define_entries.emplace_back(
                            program.identifier,
                            defines_to_json(snapshot_cfg.effective_program_defines(program, permutation.selection)));
                    }
                    Json variant_payload = Json::make_object({
                        {"cache_schema", Json::make_int(CACHE_SCHEMA_VERSION)},
                        {"compiler", Json::make_string(compiler_info.sha256)},
                        {"dependency_digest", Json::make_string(dependency_digest)},
                        {"host_abi", Json::make_string("per_variant")},
                        {"variant_set", Json::make_string(family_name)},
                        {"permutation", Json::make_string(permutation.identifier)},
                        {"selection", selection_to_json(permutation.selection)},
                        {"programs", Json::make_array(std::move(program_ids))},
                        {"defines", defines_to_json(defines)},
                        {"program_defines", Json::make_object(std::move(program_define_entries))},
                    });
                    auto variant_hostgen_key = sha256_hex(canonical_json(variant_payload));
                    std::vector<std::string> variant_interfaces;
                    std::vector<Program const *> program_ptrs;
                    for (auto const &program : programs) {
                        variant_interfaces.push_back(program.identifier + ".inl");
                        program_ptrs.push_back(&program);
                    }
                    HostVariantTask task;
                    task.family_name = family_name;
                    task.permutation_id = permutation.identifier;
                    task.variant_hostgen_key = variant_hostgen_key;
                    task.defines = defines;
                    task.selection = permutation.selection;
                    task.programs = std::move(program_ptrs);
                    task.variant_interfaces = std::move(variant_interfaces);
                    if (defines == default_defines) {
                        task.raw_host = generated_host;
                        task.materialize = false;
                    } else {
                        task.raw_host = work_dir / "per_variant" / family_name / permutation.identifier;
                        std::filesystem::create_directories(task.raw_host, ec);
                        task.materialize = true;
                    }
                    host_tasks.push_back(std::move(task));
                }
            }
            parallel_for(host_tasks.size(), [&](std::size_t index) {
                auto const &task = host_tasks[index];
                if (!task.materialize) {
                    return;
                }
                std::vector<Program> required;
                required.reserve(task.programs.size());
                for (auto const *program : task.programs) {
                    required.push_back(*program);
                }
                materialize_hostgen_tree(snapshot_cfg, compiler_path_resolved, task.variant_hostgen_key,
                                         task.defines, task.selection, required,
                                         cache_root_resolved, task.raw_host, rebuild);
            });
            for (auto const &task : host_tasks) {
                auto variant_destination = generated_host / "variants" / task.family_name / task.permutation_id;
                for (auto const &relative : task.variant_interfaces) {
                    auto source = task.raw_host / relative;
                    auto target = variant_destination / relative;
                    std::filesystem::create_directories(target.parent_path(), ec);
                    copy_if_different(source, target);
                }
            }
                for (auto const &family_entry : per_variant_families) {
                    for (auto const &program : family_entry.second) {
                        auto path = generated_host / (program.identifier + ".inl");
                        if (std::filesystem::exists(path)) {
                            remove_path(path);
                        }
                    }
                }
                auto merged_host = work_dir / "merged";
                std::filesystem::create_directories(merged_host, ec);
                if (std::filesystem::is_directory(host_output_resolved)) {
                    copy_tree_if_different(host_output_resolved, merged_host);
                }
                auto families_path = merged_host / "families";
                if (std::filesystem::exists(families_path)) {
                    remove_path(families_path);
                }
                auto variants_path = merged_host / "variants";
                if (std::filesystem::exists(variants_path)) {
                    remove_path(variants_path);
                }
                for (auto const &family_entry : per_variant_families) {
                    for (auto const &program : family_entry.second) {
                        auto path = merged_host / (program.identifier + ".inl");
                        if (std::filesystem::exists(path)) {
                            remove_path(path);
                        }
                    }
                }
                copy_tree_if_different(generated_host, merged_host);
                write_file_text(merged_host / ".shader_input_id",
                                build_input_id(dependency_digest, compiler_info) + "\n");
                auto current_config = load_config(config.manifest_path);
                if (tree_digest(current_config) != dependency_digest) {
                    throw ShaderInputsChanged();
                }
                atomic_replace_directory(merged_host, host_output_resolved);
                return host_output_resolved;
        } catch (ShaderInputsChanged const &) {
            if (attempt == 1) {
                throw ShaderVariantError(
                    "Shader inputs changed repeatedly during host generation; the previous host interfaces were preserved");
            }
        }
    }
    throw ShaderVariantError("unreachable");
}

std::filesystem::path generate_lsp(ShaderVariantConfig const &config,
                                   std::filesystem::path const &output,
                                   std::filesystem::path const &compiler_path) {
    auto output_resolved = resolve_path(output);
    std::error_code ec;
    std::filesystem::create_directories(output_resolved.parent_path(), ec);
    static std::mt19937_64 rng{std::random_device{}()};
    auto temporary = output_resolved.parent_path() /
                     ("." + output_resolved.filename().string() + "." + std::to_string(rng()) + ".tmp");
    auto command = compiler_command(config, compiler_path, config.shader_root, temporary,
                                    config.effective_defines({}), std::nullopt, std::nullopt,
                                    std::nullopt, false, true);
    run_compiler(command, output_resolved.parent_path());
    if (!std::filesystem::is_regular_file(temporary)) {
        throw ShaderVariantError("Compiler did not produce compile_commands.json");
    }
    if (std::filesystem::exists(output_resolved) && files_identical(temporary, output_resolved)) {
        std::filesystem::remove(temporary, ec);
    } else {
        std::filesystem::rename(temporary, output_resolved, ec);
        if (ec) {
            throw ShaderVariantError("Cannot write compile_commands.json: " + ec.message());
        }
    }
    return output_resolved;
}

// ---------------------------------------------------------------------------
// Variant command dispatch
// ---------------------------------------------------------------------------
int run_variant_command(VariantArgs const &args) {
    try {
        auto config = load_config(args.manifest);
        if (args.command == "validate") {
            if (!args.quiet) {
                std::cout << "Validated " << args.manifest.string() << "\n" << std::flush;
            }
            return 0;
        }
        if (args.command == "backends") {
            for (auto const &backend : config.backends) {
                std::cout << backend << "\n";
            }
            std::cout << std::flush;
            return 0;
        }
        if (args.command == "verify") {
            std::filesystem::path shader_root;
            std::optional<std::string> expected_backend;
            if (!args.shader_root.empty()) {
                shader_root = args.shader_root;
            } else {
                if (args.backends.size() != 1) {
                    throw ShaderVariantError(
                        "verify requires --shader-root or --build-root with --backend");
                }
                auto build_root = args.build_root.empty() ? default_build_root(args.project_root) : args.build_root;
                shader_root = build_root / ("shader_build_" + args.backends[0]);
                expected_backend = args.backends[0];
            }
            auto manifest = verify_shader_root(shader_root, expected_backend);
            auto backend = manifest.get("backend");
            auto build_id = manifest.get("build_id");
            if (!args.quiet) {
                std::cout << "Verified " << (backend && backend->is_string() ? backend->str : "?")
                          << " shader build "
                          << (build_id && build_id->is_string() ? build_id->str : "?") << "\n"
                          << std::flush;
            }
            return 0;
        }
        if (args.command == "verify-coherence") {
            auto build_root = args.build_root.empty() ? default_build_root(args.project_root) : args.build_root;
            auto host_output = args.host_out.empty() ? config.shader_root / "host" : args.host_out;
            auto backends = args.backends.empty() ? config.backends : args.backends;
            auto input_id = verify_shader_coherence(build_root, backends, host_output, args.plugin_marker);
            if (!args.quiet) {
                std::cout << "Verified coherent shader generation " << input_id << "\n" << std::flush;
            }
            return 0;
        }
        if (args.command == "lsp") {
            auto output = args.out.empty() ? config.shader_root / ".vscode" / "compile_commands.json" : args.out;
            auto generated = generate_lsp(config, output, args.compiler);
            if (!args.quiet) {
                std::cout << "Generated shader compile commands: " << generated.string() << "\n" << std::flush;
            }
            return 0;
        }
        if (args.command == "build") {
            auto build_root = args.build_root.empty()
                                  ? default_build_root(args.project_root)
                                  : resolve_path(args.build_root);
            auto cache_root = args.cache_root.empty()
                                  ? args.project_root / "build" / ".shader_cache" / "variants-v1"
                                  : resolve_path(args.cache_root);
            auto host_output = args.host_out.empty()
                                   ? config.shader_root / "host"
                                   : resolve_path(args.host_out);
            auto backends = args.backends.empty() ? config.backends : args.backends;
            bool hostgen_requested = args.hostgen || args.hostgen_only;

            auto compiler_info = compiler_fingerprint(args.compiler);
            auto dependency_digest = tree_digest(config);
            auto input_id = build_input_id(dependency_digest, compiler_info);

            // Fast no-op path: state marker + existing verified artifacts.
            if (!args.rebuild) {
                auto saved = load_build_state(build_root);
                if (saved.has_value()) {
                    bool matches = saved->input_id == input_id &&
                                   saved->compiler_sha256 == compiler_info.sha256 &&
                                   saved->compiler == path_casefold(args.compiler);
                    if (!backends.empty()) {
                        matches = matches && saved->backends == backends;
                    }
                    if (matches) {
                        if (hostgen_requested) {
                            matches = saved->host_out == path_casefold(resolve_path(host_output));
                        }
                    }
                    if (matches) {
                        try {
                            if (!args.hostgen_only) {
                                for (auto const &backend : backends) {
                                    verify_shader_root(build_root / ("shader_build_" + backend), backend);
                                }
                            }
                            if (hostgen_requested) {
                                auto marker = host_output / HOST_INPUT_ID_MARKER;
                                if (read_input_id_marker(marker, "hostgen") != input_id) {
                                    matches = false;
                                }
                            }
                        } catch (std::exception const &) {
                            matches = false;
                        }
                    }
                    if (matches) {
                        return 0;
                    }
                }
            }

            if (!args.hostgen_only) {
                for (auto const &backend : backends) {
                    auto output = build_backend(config, backend, build_root, cache_root, args.compiler,
                                                args.rebuild);
                    if (!args.quiet) {
                        std::cout << "Built " << backend << " shaders: " << output.string() << "\n" << std::flush;
                    }
                }
            }
            if (hostgen_requested) {
                auto output = build_hostgen(config, host_output, cache_root, args.compiler, args.rebuild);
                if (!args.quiet) {
                    std::cout << "Generated shader host headers: " << output.string() << "\n" << std::flush;
                }
            }
            BuildState state;
            state.input_id = input_id;
            state.compiler_sha256 = compiler_info.sha256;
            state.compiler = path_casefold(args.compiler);
            state.backends = backends;
            if (hostgen_requested) {
                state.host_out = path_casefold(resolve_path(host_output));
            } else {
                // A backend-only build must not invalidate a later hostgen-only
                // no-op: keep the previously published host output marker.
                auto previous = load_build_state(build_root);
                if (previous.has_value()) {
                    state.host_out = previous->host_out;
                }
            }
            save_build_state(build_root, state);
            return 0;
        }
        throw ShaderVariantError("Unknown variant command: " + args.command);
    } catch (std::exception const &error) {
        std::cerr << "shader-build: error: " << error.what() << "\n";
        return 1;
    }
}

} // namespace rbc_shader
