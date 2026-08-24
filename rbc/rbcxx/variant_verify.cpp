#include "variant_verify.h"
#include "sha256.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace rbc_shader {

namespace {

bool sha256_hex_regex(std::string const &value) {
    if (value.size() != 64) {
        return false;
    }
    for (char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool identifier_regex(std::string const &value) {
    if (value.empty() || value[0] < 'a' || value[0] > 'z') {
        return false;
    }
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
            continue;
        }
        return false;
    }
    return true;
}

Json const &require_object(Json const &value, std::string const &context) {
    if (!value.is_object()) {
        throw ShaderVariantError(context + " must be an object");
    }
    return value;
}

Json const &require_list(Json const &value, std::string const &context) {
    if (!value.is_array()) {
        throw ShaderVariantError(context + " must be an array");
    }
    return value;
}

std::string require_string(Json const &value, std::string const &context) {
    if (!value.is_string() || value.str.empty()) {
        throw ShaderVariantError(context + " must be a non-empty string");
    }
    return value.str;
}

Json const &require_key_object(Json const &obj, std::string_view key, std::string const &context) {
    auto value = obj.get(key);
    if (value == nullptr) {
        throw ShaderVariantError(context + " must be an object");
    }
    return require_object(*value, context);
}

Json const &require_key_list(Json const &obj, std::string_view key) {
    auto value = obj.get(key);
    if (value == nullptr) {
        throw ShaderVariantError(std::string(key) + " must be an array");
    }
    return require_list(*value, std::string(key));
}

std::string require_key_string(Json const &obj, std::string_view key, std::string const &context) {
    auto value = obj.get(key);
    if (value == nullptr) {
        throw ShaderVariantError(context + " must be a non-empty string");
    }
    return require_string(*value, context);
}

void check_keys(Json const &value, std::set<std::string> const &allowed, std::string const &context) {
    std::vector<std::string> unknown;
    for (auto const &entry : value.obj) {
        if (allowed.find(entry.first) == allowed.end()) {
            unknown.push_back(entry.first);
        }
    }
    if (!unknown.empty()) {
        std::sort(unknown.begin(), unknown.end());
        std::string joined;
        for (std::size_t i = 0; i < unknown.size(); ++i) {
            if (i != 0) {
                joined += ", ";
            }
            joined += unknown[i];
        }
        throw ShaderVariantError("Unknown " + context + " keys: " + joined);
    }
}

std::string safe_relative(std::string const &value, std::string const &context) {
    if (value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        throw ShaderVariantError(context + " must use a project-relative POSIX path");
    }
    if (value.empty() || value.front() == '/') {
        throw ShaderVariantError("Unsafe " + context + ": " + value);
    }
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto slash = value.find('/', start);
        if (slash == std::string::npos) {
            parts.push_back(value.substr(start));
            break;
        }
        parts.push_back(value.substr(start, slash - start));
        start = slash + 1;
    }
    for (auto const &part : parts) {
        if (part.empty() || part == "." || part == "..") {
            throw ShaderVariantError("Unsafe " + context + ": " + value);
        }
    }
    return value;
}

std::string identifier(Json const &value, std::string const &context) {
    auto result = require_string(value, context);
    if (!identifier_regex(result)) {
        throw ShaderVariantError("Invalid " + context + ": " + result);
    }
    return result;
}

std::map<std::string, std::string> runtime_selection(Json const &value, std::string const &context) {
    require_object(value, context);
    std::map<std::string, std::string> selection;
    for (auto const &entry : value.obj) {
        auto dimension = identifier(Json::make_string(entry.first), context + " dimension");
        auto selected_value = identifier(entry.second, context + " value");
        selection[dimension] = selected_value;
    }
    return selection;
}

std::vector<std::string> runtime_feature_list(Json const &value, std::string const &context) {
    auto const &raw = require_list(value, context);
    std::vector<std::string> features;
    for (auto const &feature_value : raw.arr) {
        auto feature = identifier(feature_value, context);
        if (std::find(features.begin(), features.end(), feature) != features.end()) {
            throw ShaderVariantError("Duplicate scene feature in " + context + ": " + feature);
        }
        features.push_back(feature);
    }
    std::sort(features.begin(), features.end());
    std::vector<std::string> raw_strings;
    for (auto const &item : raw.arr) {
        raw_strings.push_back(item.str);
    }
    if (raw_strings != features) {
        throw ShaderVariantError(context + " must be sorted");
    }
    return features;
}

bool rule_matches(std::map<std::string, std::string> const &selection,
                  std::vector<std::string> const &required_features,
                  std::vector<std::string> const &forbidden_features,
                  std::vector<std::string> const &enabled) {
    (void)selection;
    for (auto const &feature : required_features) {
        if (std::find(enabled.begin(), enabled.end(), feature) == enabled.end()) {
            return false;
        }
    }
    for (auto const &feature : forbidden_features) {
        if (std::find(enabled.begin(), enabled.end(), feature) != enabled.end()) {
            return false;
        }
    }
    return true;
}

struct RuntimeRule {
    std::map<std::string, std::string> selection;
    std::vector<std::string> required_features;
    std::vector<std::string> forbidden_features;
};

void validate_variant_rules(std::string const &variant_set, std::vector<RuntimeRule> const &rules) {
    std::set<std::string> involved;
    for (auto const &rule : rules) {
        involved.insert(rule.required_features.begin(), rule.required_features.end());
        involved.insert(rule.forbidden_features.begin(), rule.forbidden_features.end());
    }
    std::vector<std::string> involved_features(involved.begin(), involved.end());
    if (involved_features.size() > static_cast<std::size_t>(MAX_SCENE_FEATURES_PER_FAMILY)) {
        throw ShaderVariantError(
            "Variant set " + variant_set + " uses " + std::to_string(involved_features.size()) +
            " scene features; exhaustive validation supports at most " +
            std::to_string(MAX_SCENE_FEATURES_PER_FAMILY));
    }
    std::size_t combinations = std::size_t{1} << involved_features.size();
    for (std::size_t mask = 0; mask < combinations; ++mask) {
        std::vector<std::string> enabled;
        for (std::size_t i = 0; i < involved_features.size(); ++i) {
            if ((mask >> i) & 1u) {
                enabled.push_back(involved_features[i]);
            }
        }
        std::vector<std::size_t> matches;
        for (std::size_t i = 0; i < rules.size(); ++i) {
            if (rule_matches(rules[i].selection, rules[i].required_features,
                             rules[i].forbidden_features, enabled)) {
                matches.push_back(i);
            }
        }
        if (matches.size() == 1) {
            continue;
        }
        std::string feature_description;
        for (std::size_t i = 0; i < enabled.size(); ++i) {
            if (i != 0) {
                feature_description += ", ";
            }
            feature_description += enabled[i];
        }
        if (feature_description.empty()) {
            feature_description = "<none>";
        }
        if (!matches.empty()) {
            std::string selections;
            for (std::size_t i = 0; i < matches.size(); ++i) {
                if (i != 0) {
                    selections += ", ";
                }
                selections += canonical_selection(rules[matches[i]].selection);
            }
            throw ShaderVariantError(
                "Variant set " + variant_set + " has ambiguous scene feature rules for [" +
                feature_description + "]: " + selections);
        }
        throw ShaderVariantError(
            "Variant set " + variant_set + " has no scene feature rule for [" +
            feature_description + "]");
    }
}

} // namespace

Json verify_shader_root(std::filesystem::path const &shader_root,
                        std::optional<std::string> const &expected_backend) {
    auto root = std::filesystem::weakly_canonical(shader_root);
    auto manifest_path = root / RUNTIME_MANIFEST_NAME;
    Json manifest;
    try {
        manifest = read_json_file(manifest_path, "runtime shader manifest");
    } catch (std::exception const &error) {
        throw ShaderVariantError(std::string("Cannot read runtime shader manifest: ") + error.what());
    }
    require_object(manifest, "runtime manifest");
    for (auto const &required : {"schema_version", "backend", "build_id", "input_id", "programs", "families"}) {
        if (manifest.get(required) == nullptr) {
            throw ShaderVariantError("Runtime shader manifest is missing " + std::string(required));
        }
    }
    auto schema_value = manifest.get("schema_version");
    if (!schema_value->is_int() || schema_value->integer != RUNTIME_SCHEMA_VERSION) {
        std::string schema_text = schema_value->is_int() ? std::to_string(schema_value->integer) : schema_value->str;
        throw ShaderVariantError("Unsupported runtime shader schema: " + schema_text);
    }
    auto backend = require_string(*manifest.get("backend"), "runtime backend");
    if (expected_backend.has_value() && backend != *expected_backend) {
        throw ShaderVariantError("Runtime manifest backend is " + backend + ", expected " + *expected_backend);
    }
    require_string(*manifest.get("build_id"), "runtime build_id");
    auto input_id = require_string(*manifest.get("input_id"), "runtime input_id");
    if (!sha256_hex_regex(input_id)) {
        throw ShaderVariantError("Runtime input_id must be a lowercase SHA-256 digest");
    }
    auto const &programs = require_object(*manifest.get("programs"), "runtime programs");
    if (programs.obj.empty()) {
        throw ShaderVariantError("Runtime manifest has no programs");
    }

    std::set<std::string> artifacts;
    std::map<std::string, std::set<std::string>> program_selections;
    std::map<std::string, std::map<std::string, std::string>> program_defaults;

    for (auto const &program_entry : programs.obj) {
        auto logical = safe_relative(program_entry.first, "runtime program id");
        auto const &program = require_object(program_entry.second, "runtime program " + logical);
        check_keys(program, {"default_selection", "variants"}, "runtime program " + logical);
        auto default_selection = runtime_selection(require_key_object(program, "default_selection",
                                                                     "runtime program " + logical + " default_selection"),
                                                   "runtime program " + logical + " default_selection");
        auto const &variants = require_key_list(program, "variants");
        if (variants.arr.empty()) {
            throw ShaderVariantError("Runtime program " + logical + " has no variants");
        }
        bool found_default = false;
        std::set<std::string> selection_keys;
        for (auto const &variant_value : variants.arr) {
            auto const &variant = require_object(variant_value, "runtime program " + logical + " variant");
            auto selection = runtime_selection(require_key_object(variant, "selection",
                                                                  "runtime program " + logical + " variant selection"),
                                               "runtime program " + logical + " variant selection");
            std::set<std::string> allowed_variant_keys = {
                "selection", "artifact", "sha256", "size", "compile_key"};
            if (!selection.empty()) {
                allowed_variant_keys.insert("canonical_key");
            }
            check_keys(variant, allowed_variant_keys, "runtime program " + logical + " variant");
            auto canonical = canonical_selection(selection);
            if (selection_keys.find(canonical) != selection_keys.end()) {
                throw ShaderVariantError(
                    "Runtime program " + logical + " has duplicate selection " + canonical);
            }
            selection_keys.insert(canonical);
            if (selection == default_selection) {
                found_default = true;
            }
            if (!selection.empty()) {
                auto canonical_key = variant.get("canonical_key");
                if (!canonical_key || !canonical_key->is_string() || canonical_key->str != canonical) {
                    throw ShaderVariantError(
                        "Runtime program " + logical + " has invalid canonical_key for " + canonical);
                }
            }
            auto compile_key = require_key_string(variant, "compile_key",
                                                  "runtime program " + logical + " variant compile_key");
            if (!sha256_hex_regex(compile_key)) {
                throw ShaderVariantError(
                    "Invalid runtime compile_key for " + logical + " (" + canonical + ")");
            }
            auto artifact = safe_relative(
                require_key_string(variant, "artifact", "runtime program " + logical + " variant artifact"),
                "runtime program " + logical + " artifact");
            if (artifacts.find(artifact) != artifacts.end()) {
                throw ShaderVariantError("Duplicate runtime artifact: " + artifact);
            }
            artifacts.insert(artifact);
            auto artifact_path = std::filesystem::weakly_canonical(root / artifact);
            auto relative_check = artifact_path.lexically_relative(root);
            if (relative_check.empty() || relative_check.string().find("..") != std::string::npos) {
                throw ShaderVariantError("Artifact escapes shader root: " + artifact);
            }
            if (!std::filesystem::is_regular_file(artifact_path)) {
                throw ShaderVariantError("Missing runtime shader artifact: " + artifact);
            }
            auto size_value = variant.get("size");
            if (!size_value || !size_value->is_int() || size_value->integer < 0) {
                throw ShaderVariantError("Invalid artifact size for " + artifact);
            }
            std::error_code ec;
            auto actual_size = std::filesystem::file_size(artifact_path, ec);
            if (ec || actual_size != static_cast<std::uintmax_t>(size_value->integer)) {
                throw ShaderVariantError("Artifact size mismatch: " + artifact);
            }
            auto hash_value = variant.get("sha256");
            if (!hash_value || !hash_value->is_string() || !sha256_hex_regex(hash_value->str) ||
                sha256_file(artifact_path) != hash_value->str) {
                throw ShaderVariantError("Artifact hash mismatch: " + artifact);
            }
        }
        if (!found_default) {
            throw ShaderVariantError(
                "Runtime program " + logical + " default_selection has no matching variant");
        }
        program_selections[logical] = selection_keys;
        program_defaults[logical] = default_selection;
    }

    auto const &families = require_object(*manifest.get("families"), "runtime families");
    std::set<std::string> family_programs;
    for (auto const &family_entry : families.obj) {
        auto family_name = identifier(Json::make_string(family_entry.first), "runtime family name");
        auto const &family = require_object(family_entry.second, "runtime family " + family_name);
        check_keys(family, {"programs", "rules"}, "runtime family " + family_name);
        std::vector<std::string> members;
        auto const &members_value = require_key_list(family, "programs");
        for (auto const &member_value : members_value.arr) {
            auto member = safe_relative(require_string(member_value, "runtime family " + family_name + " program"),
                                        "runtime family " + family_name + " program");
            if (std::find(members.begin(), members.end(), member) != members.end()) {
                throw ShaderVariantError("Runtime family " + family_name + " repeats program " + member);
            }
            if (programs.get(member) == nullptr) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " references unknown program " + member);
            }
            if (family_programs.find(member) != family_programs.end()) {
                throw ShaderVariantError(
                    "Runtime program " + member + " belongs to multiple families");
            }
            members.push_back(member);
            family_programs.insert(member);
        }
        if (members.empty()) {
            throw ShaderVariantError("Runtime family " + family_name + " contains no programs");
        }

        std::vector<RuntimeRule> rules;
        std::optional<std::set<std::string>> selection_dimensions;
        std::set<std::string> rule_selections;
        auto const &rules_value = require_key_list(family, "rules");
        for (auto const &rule_value : rules_value.arr) {
            auto const &rule_raw = require_object(rule_value, "runtime family " + family_name + " rule");
            check_keys(rule_raw, {"selection", "required_features", "forbidden_features"},
                       "runtime family " + family_name + " rule");
            auto selection = runtime_selection(require_key_object(rule_raw, "selection",
                                                                   "runtime family " + family_name + " rule selection"),
                                               "runtime family " + family_name + " rule selection");
            if (selection.empty()) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " rules must select a dimension");
            }
            std::set<std::string> current_dimensions;
            for (auto const &entry : selection) {
                current_dimensions.insert(entry.first);
            }
            if (!selection_dimensions.has_value()) {
                selection_dimensions = current_dimensions;
            } else if (current_dimensions != *selection_dimensions) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " rules select different dimensions");
            }
            auto canonical = canonical_selection(selection);
            if (rule_selections.find(canonical) != rule_selections.end()) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " repeats selection " + canonical);
            }
            rule_selections.insert(canonical);
            RuntimeRule rule;
            rule.selection = selection;
            rule.required_features = runtime_feature_list(
                require_key_list(rule_raw, "required_features"),
                "runtime family " + family_name + " required_features");
            rule.forbidden_features = runtime_feature_list(
                require_key_list(rule_raw, "forbidden_features"),
                "runtime family " + family_name + " forbidden_features");
            std::set<std::string> conflicts;
            for (auto const &feature : rule.required_features) {
                if (std::find(rule.forbidden_features.begin(), rule.forbidden_features.end(), feature) !=
                    rule.forbidden_features.end()) {
                    conflicts.insert(feature);
                }
            }
            if (!conflicts.empty()) {
                std::string joined;
                std::size_t i = 0;
                for (auto const &feature : conflicts) {
                    if (i != 0) {
                        joined += ", ";
                    }
                    joined += feature;
                    ++i;
                }
                throw ShaderVariantError(
                    "Runtime family " + family_name + " both requires and forbids: " + joined);
            }
            rules.push_back(std::move(rule));
        }
        if (rules.empty()) {
            throw ShaderVariantError("Runtime family " + family_name + " has no rules");
        }
        validate_variant_rules(family_name, rules);
        for (auto const &member : members) {
            auto selections_iter = program_selections.find(member);
            if (selections_iter == program_selections.end() || selections_iter->second != rule_selections) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " rules do not match artifacts for program " + member);
            }
            auto defaults_iter = program_defaults.find(member);
            if (defaults_iter == program_defaults.end() ||
                rule_selections.find(canonical_selection(defaults_iter->second)) == rule_selections.end()) {
                throw ShaderVariantError(
                    "Runtime family " + family_name + " default does not match program " + member + " artifacts");
            }
        }
    }

    for (auto const &program_entry : programs.obj) {
        auto logical = program_entry.first;
        if (family_programs.find(logical) != family_programs.end()) {
            continue;
        }
        auto const &defaults = program_defaults[logical];
        auto const &selections = program_selections[logical];
        bool only_default = selections.size() == 1 && selections.find("default") != selections.end();
        if (!defaults.empty() || !only_default) {
            throw ShaderVariantError(
                "Runtime program " + logical + " has variants but belongs to no family");
        }
    }
    return manifest;
}

std::map<std::string, Json> verify_shader_backends(std::filesystem::path const &build_root,
                                                   std::vector<std::string> const &backends) {
    auto root = std::filesystem::weakly_canonical(build_root);
    std::map<std::string, Json> verified;
    for (auto const &backend : backends) {
        verified[backend] = verify_shader_root(root / ("shader_build_" + backend), backend);
    }
    std::set<std::string> input_ids;
    for (auto const &entry : verified) {
        auto input_id = entry.second.get("input_id");
        if (input_id != nullptr && input_id->is_string()) {
            input_ids.insert(input_id->str);
        }
    }
    if (input_ids.size() > 1) {
        std::string generations;
        bool first = true;
        for (auto const &entry : verified) {
            if (!first) {
                generations += ", ";
            }
            first = false;
            auto input_id = entry.second.get("input_id");
            generations += entry.first + "=" + (input_id && input_id->is_string() ? input_id->str : "?");
        }
        throw ShaderVariantError(
            "Shader backends were built from different inputs: " + generations);
    }
    return verified;
}

std::string read_input_id_marker(std::filesystem::path const &path, std::string const &context) {
    std::string input_id;
    try {
        input_id = read_file_text(path);
    } catch (std::exception const &error) {
        throw ShaderVariantError(
            "Cannot read " + context + " input marker " + path.string() + ": " + error.what());
    }
    while (!input_id.empty() && (input_id.back() == '\r' || input_id.back() == '\n' || input_id.back() == ' ')) {
        input_id.pop_back();
    }
    if (!sha256_hex_regex(input_id)) {
        throw ShaderVariantError(
            context + " input marker must contain a lowercase SHA-256 digest: " + path.string());
    }
    return input_id;
}

std::string verify_shader_coherence(std::filesystem::path const &build_root,
                                    std::vector<std::string> const &backends,
                                    std::filesystem::path const &host_output,
                                    std::optional<std::filesystem::path> const &plugin_marker) {
    auto backend_list = backends;
    if (backend_list.empty()) {
        throw ShaderVariantError("At least one shader backend is required");
    }
    auto manifests = verify_shader_backends(build_root, backend_list);
    auto expected_iter = manifests.find(backend_list[0]);
    if (expected_iter == manifests.end()) {
        throw ShaderVariantError("Missing verified backend " + backend_list[0]);
    }
    auto expected_value = expected_iter->second.get("input_id");
    if (!expected_value || !expected_value->is_string()) {
        throw ShaderVariantError("Missing input_id in verified backend " + backend_list[0]);
    }
    std::string expected = expected_value->str;

    std::map<std::string, std::string> components;
    for (auto const &entry : manifests) {
        auto input_id = entry.second.get("input_id");
        components["backend " + entry.first] = input_id && input_id->is_string() ? input_id->str : "";
    }
    components["hostgen"] = read_input_id_marker(
        std::filesystem::weakly_canonical(host_output) / HOST_INPUT_ID_MARKER, "hostgen");
    if (plugin_marker.has_value()) {
        components["render plugin"] = read_input_id_marker(
            std::filesystem::weakly_canonical(*plugin_marker), "render plugin");
    }
    std::vector<std::string> mismatched_components;
    for (auto const &entry : components) {
        if (entry.second != expected) {
            mismatched_components.push_back(entry.first);
        }
    }
    if (!mismatched_components.empty()) {
        std::string generations;
        bool first = true;
        for (auto const &entry : components) {
            if (!first) {
                generations += ", ";
            }
            first = false;
            generations += entry.first + "=" + entry.second;
        }
        throw ShaderVariantError("Shader build generations do not match: " + generations);
    }
    return expected;
}

} // namespace rbc_shader
