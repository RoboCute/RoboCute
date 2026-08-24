#include "variant_config.h"
#include "sha256.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace rbc_shader {

namespace {

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

bool backend_regex(std::string const &value) {
    if (value.empty() || value[0] < 'a' || value[0] > 'z') {
        return false;
    }
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            continue;
        }
        return false;
    }
    return true;
}

bool macro_regex(std::string const &value) {
    if (value.empty()) {
        return false;
    }
    char first = value[0];
    if (!(std::isalpha(static_cast<unsigned char>(first)) || first == '_')) {
        return false;
    }
    for (char c : value) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
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

std::string identifier(std::string const &value, std::string const &context) {
    if (!identifier_regex(value)) {
        throw ShaderVariantError("Invalid " + context + ": " + value);
    }
    return value;
}

std::vector<Define> parse_defines(Json const &value, std::string const &context) {
    require_object(value, context);
    std::vector<std::pair<std::string, Json const *>> raw_entries;
    for (auto const &entry : value.obj) {
        raw_entries.emplace_back(entry.first, &entry.second);
    }
    std::sort(raw_entries.begin(), raw_entries.end(), [](auto const &a, auto const &b) {
        return a.first < b.first;
    });
    std::vector<Define> defines;
    for (auto const &entry : raw_entries) {
        auto const &macro = entry.first;
        if (!macro_regex(macro)) {
            throw ShaderVariantError("Invalid macro name in " + context + ": " + macro);
        }
        auto const &macro_value = *entry.second;
        if (!macro_value.is_null() && !macro_value.is_string()) {
            throw ShaderVariantError("Macro value for " + macro + " must be a string or null");
        }
        defines.emplace_back(macro,
                             macro_value.is_string() ? std::optional<std::string>(macro_value.str)
                                                     : std::optional<std::string>());
    }
    return defines;
}

std::vector<std::string> parse_feature_list(Json const &value, std::string const &context) {
    require_list(value, context);
    std::vector<std::string> features;
    for (auto const &feature_value : value.arr) {
        auto feature = identifier(feature_value, context);
        if (std::find(features.begin(), features.end(), feature) != features.end()) {
            throw ShaderVariantError("Duplicate scene feature in " + context + ": " + feature);
        }
        features.push_back(feature);
    }
    std::sort(features.begin(), features.end());
    return features;
}

SceneFeatures parse_scene_features(Json const &value, std::string const &context) {
    require_object(value, context);
    check_keys(value, {"required", "forbidden"}, context);
    SceneFeatures result;
    Json const empty_list = Json::make_array({});
    auto required_value = value.get("required");
    auto forbidden_value = value.get("forbidden");
    result.required = parse_feature_list(required_value ? *required_value : empty_list,
                                         context + " required");
    result.forbidden = parse_feature_list(forbidden_value ? *forbidden_value : empty_list,
                                          context + " forbidden");
    std::set<std::string> required_set(result.required.begin(), result.required.end());
    std::set<std::string> forbidden_set(result.forbidden.begin(), result.forbidden.end());
    std::vector<std::string> conflicts;
    std::set_intersection(required_set.begin(), required_set.end(),
                          forbidden_set.begin(), forbidden_set.end(),
                          std::back_inserter(conflicts));
    if (!conflicts.empty()) {
        std::string joined;
        for (std::size_t i = 0; i < conflicts.size(); ++i) {
            if (i != 0) {
                joined += ", ";
            }
            joined += conflicts[i];
        }
        throw ShaderVariantError(context + " both requires and forbids: " + joined);
    }
    return result;
}

bool rule_matches(VariantRule const &rule, std::vector<std::string> const &enabled) {
    for (auto const &feature : rule.required_features) {
        if (std::find(enabled.begin(), enabled.end(), feature) == enabled.end()) {
            return false;
        }
    }
    for (auto const &feature : rule.forbidden_features) {
        if (std::find(enabled.begin(), enabled.end(), feature) != enabled.end()) {
            return false;
        }
    }
    return true;
}

void validate_variant_rules(std::string const &variant_set, std::vector<VariantRule> const &rules) {
    std::set<std::string> involved;
    for (auto const &rule : rules) {
        involved.insert(rule.required_features.begin(), rule.required_features.end());
        involved.insert(rule.forbidden_features.begin(), rule.forbidden_features.end());
    }
    std::vector<std::string> involved_features(involved.begin(), involved.end());
    if (involved_features.size() > static_cast<std::size_t>(MAX_SCENE_FEATURES_PER_FAMILY)) {
        throw ShaderVariantError(
            "Variant set " + variant_set + " uses " +
            std::to_string(involved_features.size()) +
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
        std::vector<VariantRule const *> matches;
        for (auto const &rule : rules) {
            if (rule_matches(rule, enabled)) {
                matches.push_back(&rule);
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
                selections += canonical_selection(matches[i]->selection);
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

std::string join_sorted(std::set<std::string> const &values) {
    std::string joined;
    std::size_t i = 0;
    for (auto const &value : values) {
        if (i != 0) {
            joined += ", ";
        }
        joined += value;
        ++i;
    }
    return joined;
}

} // namespace

bool VariantRule::matches(std::vector<std::string> const &enabled) const {
    return rule_matches(*this, enabled);
}

std::string canonical_selection(std::map<std::string, std::string> const &selection) {
    if (selection.empty()) {
        return "default";
    }
    std::string result;
    bool first = true;
    for (auto const &entry : selection) {
        if (!first) {
            result += "+";
        }
        first = false;
        result += entry.first;
        result += "=";
        result += entry.second;
    }
    return result;
}

std::map<std::string, Program> ShaderVariantConfig::program_map() const {
    std::map<std::string, Program> result;
    for (auto const &program : programs) {
        result[program.identifier] = program;
    }
    return result;
}

std::vector<Program> ShaderVariantConfig::family_programs(std::string const &variant_set) const {
    std::vector<Program> result;
    for (auto const &program : programs) {
        if (program.variant_set == variant_set) {
            result.push_back(program);
        }
    }
    return result;
}

std::map<std::string, std::string> ShaderVariantConfig::default_selection() const {
    std::map<std::string, std::string> result;
    for (auto const &entry : dimensions) {
        result[entry.first] = entry.second.default_value;
    }
    return result;
}

std::map<std::string, std::string> ShaderVariantConfig::effective_selection(
    std::map<std::string, std::string> const &selection) const {
    auto result = default_selection();
    for (auto const &entry : selection) {
        result[entry.first] = entry.second;
    }
    return result;
}

std::vector<Define> ShaderVariantConfig::effective_defines(
    std::map<std::string, std::string> const &selection) const {
    auto selected_values = effective_selection(selection);
    std::map<std::string, std::optional<std::string>> merged;
    std::map<std::string, std::string> owners;
    for (auto const &define : common_defines) {
        merged[define.first] = define.second;
        owners[define.first] = "compile.defines";
    }
    for (auto const &dimension_entry : dimensions) {
        auto const &dimension_name = dimension_entry.first;
        auto const &dimension = dimension_entry.second;
        auto value_iter = selected_values.find(dimension_name);
        if (value_iter == selected_values.end()) {
            throw ShaderVariantError("Missing dimension value for " + dimension_name);
        }
        auto value_entry = dimension.values.find(value_iter->second);
        if (value_entry == dimension.values.end()) {
            throw ShaderVariantError("Unknown value " + value_iter->second + " for dimension " + dimension_name);
        }
        for (auto const &define : value_entry->second.defines) {
            if (merged.find(define.first) != merged.end()) {
                throw ShaderVariantError(
                    "Macro " + define.first + " is defined by both " + owners[define.first] +
                    " and dimension " + dimension_name);
            }
            merged[define.first] = define.second;
            owners[define.first] = "dimension " + dimension_name;
        }
    }
    std::vector<Define> result;
    for (auto const &entry : merged) {
        result.emplace_back(entry.first, entry.second);
    }
    return result;
}

std::vector<Define> ShaderVariantConfig::effective_program_defines(
    Program const &program, std::map<std::string, std::string> const &selection) const {
    std::map<std::string, std::optional<std::string>> merged;
    for (auto const &define : effective_defines(selection)) {
        merged[define.first] = define.second;
    }
    for (auto const &define : program.defines) {
        if (merged.find(define.first) != merged.end()) {
            throw ShaderVariantError(
                "Macro " + define.first + " is defined by compile/dimension defines and program " +
                program.identifier + " defines");
        }
        merged[define.first] = define.second;
    }
    std::vector<Define> result;
    for (auto const &entry : merged) {
        result.emplace_back(entry.first, entry.second);
    }
    return result;
}

VariantRule ShaderVariantConfig::variant_rule(std::string const &variant_set,
                                              Permutation const &permutation) const {
    std::set<std::string> required;
    std::set<std::string> forbidden;
    for (auto const &entry : permutation.selection) {
        auto const &dimension_name = entry.first;
        auto const &value_name = entry.second;
        auto dimension_iter = dimensions.find(dimension_name);
        if (dimension_iter == dimensions.end()) {
            throw ShaderVariantError("Unknown dimension " + dimension_name);
        }
        auto value_iter = dimension_iter->second.values.find(value_name);
        if (value_iter == dimension_iter->second.values.end()) {
            throw ShaderVariantError("Unknown value " + value_name + " for dimension " + dimension_name);
        }
        required.insert(value_iter->second.scene_features.required.begin(),
                        value_iter->second.scene_features.required.end());
        forbidden.insert(value_iter->second.scene_features.forbidden.begin(),
                         value_iter->second.scene_features.forbidden.end());
    }
    std::set<std::string> conflicts;
    std::set_intersection(required.begin(), required.end(), forbidden.begin(), forbidden.end(),
                          std::inserter(conflicts, conflicts.end()));
    if (!conflicts.empty()) {
        throw ShaderVariantError(
            "Variant set " + variant_set + " permutation " + permutation.identifier +
            " both requires and forbids scene features: " + join_sorted(conflicts));
    }
    VariantRule rule;
    rule.selection = permutation.selection;
    rule.required_features.assign(required.begin(), required.end());
    rule.forbidden_features.assign(forbidden.begin(), forbidden.end());
    return rule;
}

std::vector<VariantRule> ShaderVariantConfig::variant_rules(std::string const &variant_set) const {
    auto iter = variant_sets.find(variant_set);
    if (iter == variant_sets.end()) {
        throw ShaderVariantError("Unknown variant set " + variant_set);
    }
    std::vector<VariantRule> result;
    for (auto const &permutation : iter->second.permutations) {
        result.push_back(variant_rule(variant_set, permutation));
    }
    return result;
}

std::map<std::string, std::filesystem::path> ShaderVariantConfig::source_files() const {
    std::map<std::string, std::filesystem::path> result;
    std::error_code ec;
    if (!std::filesystem::is_directory(source_dir(), ec)) {
        return result;
    }
    std::vector<std::filesystem::path> sources;
    for (auto const &entry : std::filesystem::recursive_directory_iterator(source_dir(), ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".cpp") {
            sources.push_back(entry.path());
        }
    }
    std::sort(sources.begin(), sources.end(), [](auto const &a, auto const &b) {
        return a.generic_string() < b.generic_string();
    });
    for (auto const &source : sources) {
        auto relative = source.lexically_relative(source_dir());
        relative.replace_extension();
        auto logical = path_to_posix(relative);
        result[logical] = source;
    }
    return result;
}

namespace {

ShaderVariantConfig load_config_impl(std::filesystem::path const &manifest_path) {
    auto manifest = std::filesystem::weakly_canonical(manifest_path);
    Json raw = read_json_file(manifest, "shader variant manifest");
    require_object(raw, "manifest");
    check_keys(raw,
               {"schema_version", "backends", "compile", "scene_features", "dimensions",
                "variant_sets", "programs"},
               "manifest");
    auto schema_value = raw.get("schema_version");
    if (!schema_value || !schema_value->is_int() || schema_value->integer != SOURCE_SCHEMA_VERSION) {
        std::string schema_text = schema_value && schema_value->is_int()
                                      ? std::to_string(schema_value->integer)
                                      : (schema_value && schema_value->is_string() ? schema_value->str : "null");
        throw ShaderVariantError("Unsupported shader variant schema: " + schema_text);
    }

    std::vector<std::string> backends;
    {
        auto const &backends_value = require_key_list(raw, "backends");
        for (auto const &backend_value : backends_value.arr) {
            auto backend = require_string(backend_value, "backend");
            if (!backend_regex(backend)) {
                throw ShaderVariantError("Invalid backend: " + backend);
            }
            if (std::find(backends.begin(), backends.end(), backend) != backends.end()) {
                throw ShaderVariantError("Duplicate backend: " + backend);
            }
            backends.push_back(backend);
        }
        if (backends.empty()) {
            throw ShaderVariantError("At least one backend is required");
        }
    }

    ShaderVariantConfig config;
    config.manifest_path = manifest;
    config.shader_root = manifest.parent_path();
    config.backends = backends;

    {
        auto const &compile_raw = require_key_object(raw, "compile", "compile");
        check_keys(compile_raw, {"source_root", "include_dirs", "optimization", "defines"}, "compile");
        config.source_root = safe_relative(
            require_key_string(compile_raw, "source_root", "compile.source_root"), "source root");
        auto const &include_value = require_key_list(compile_raw, "include_dirs");
        for (auto const &include_item : include_value.arr) {
            config.include_dirs.push_back(safe_relative(
                require_string(include_item, "include directory"), "include directory"));
        }
        if (config.include_dirs.empty()) {
            throw ShaderVariantError("At least one shader include directory is required");
        }
        auto optimization = require_key_string(compile_raw, "optimization", "compile.optimization");
        std::transform(optimization.begin(), optimization.end(), optimization.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (optimization != "on" && optimization != "off") {
            throw ShaderVariantError("compile.optimization must be 'on' or 'off'");
        }
        config.optimization = optimization;
        Json const empty_object = Json::make_object({});
        auto defines_value = compile_raw.get("defines");
        config.common_defines = parse_defines(defines_value ? *defines_value : empty_object,
                                              "compile.defines");
    }

    {
        auto const &scene_value = require_key_list(raw, "scene_features");
        config.scene_features = parse_feature_list(scene_value, "scene_features");
    }
    std::set<std::string> declared_scene_features(config.scene_features.begin(),
                                                  config.scene_features.end());

    {
        auto const &dimensions_value = require_key_object(raw, "dimensions", "dimensions");
        std::vector<std::pair<std::string, Json const *>> sorted_dimensions;
        for (auto const &entry : dimensions_value.obj) {
            sorted_dimensions.emplace_back(entry.first, &entry.second);
        }
        std::sort(sorted_dimensions.begin(), sorted_dimensions.end(), [](auto const &a, auto const &b) {
            return a.first < b.first;
        });
        for (auto const &dimension_entry : sorted_dimensions) {
            auto name = identifier(Json::make_string(dimension_entry.first), "dimension name");
            auto const &dimension_raw = require_object(*dimension_entry.second, "dimension " + name);
            check_keys(dimension_raw, {"default", "values"}, "dimension " + name);
            auto default_value = identifier(require_key_string(dimension_raw, "default",
                                                               "dimension " + name + " default"),
                                            "dimension " + name + " default");
            auto const &values_value = require_key_object(dimension_raw, "values",
                                                          "dimension " + name + " values");
            Dimension dimension;
            std::vector<std::pair<std::string, Json const *>> sorted_values;
            for (auto const &entry : values_value.obj) {
                sorted_values.emplace_back(entry.first, &entry.second);
            }
            std::sort(sorted_values.begin(), sorted_values.end(), [](auto const &a, auto const &b) {
                return a.first < b.first;
            });
            for (auto const &value_entry : sorted_values) {
                auto value_name = identifier(Json::make_string(value_entry.first), "dimension " + name + " value");
                auto const &value_raw = require_object(*value_entry.second,
                                                       "dimension " + name + " value " + value_name);
                check_keys(value_raw, {"defines", "scene_features"},
                           "dimension " + name + " value " + value_name);
                DimensionValue parsed;
                Json const empty_object = Json::make_object({});
                auto defines_value = value_raw.get("defines");
                parsed.defines = parse_defines(defines_value ? *defines_value : empty_object,
                                               "dimension " + name + " value " + value_name + " defines");
                auto features_value = value_raw.get("scene_features");
                parsed.scene_features = parse_scene_features(
                    features_value ? *features_value : empty_object,
                    "dimension " + name + " value " + value_name + " scene_features");
                std::set<std::string> unknown;
                for (auto const &feature : parsed.scene_features.required) {
                    if (declared_scene_features.find(feature) == declared_scene_features.end()) {
                        unknown.insert(feature);
                    }
                }
                for (auto const &feature : parsed.scene_features.forbidden) {
                    if (declared_scene_features.find(feature) == declared_scene_features.end()) {
                        unknown.insert(feature);
                    }
                }
                if (!unknown.empty()) {
                    throw ShaderVariantError(
                        "Dimension " + name + " value " + value_name +
                        " references undeclared scene features: " + join_sorted(unknown));
                }
                dimension.values[value_name] = std::move(parsed);
            }
            if (dimension.values.find(default_value) == dimension.values.end()) {
                throw ShaderVariantError("Default value " + default_value + " is missing from dimension " + name);
            }
            dimension.default_value = default_value;
            config.dimensions[name] = std::move(dimension);
        }
    }

    {
        auto const &variant_sets_value = require_key_object(raw, "variant_sets", "variant_sets");
        std::vector<std::pair<std::string, Json const *>> sorted_sets;
        for (auto const &entry : variant_sets_value.obj) {
            sorted_sets.emplace_back(entry.first, &entry.second);
        }
        std::sort(sorted_sets.begin(), sorted_sets.end(), [](auto const &a, auto const &b) {
            return a.first < b.first;
        });
        for (auto const &set_entry : sorted_sets) {
            auto set_name = identifier(Json::make_string(set_entry.first), "variant set name");
            auto const &set_raw = require_object(*set_entry.second, "variant set " + set_name);
            check_keys(set_raw, {"default", "permutations"}, "variant set " + set_name);
            auto default_permutation = identifier(
                require_key_string(set_raw, "default", "variant set " + set_name + " default"),
                "variant set " + set_name + " default");
            auto const &permutations_value = require_key_list(set_raw, "permutations");
            VariantSet variant_set;
            variant_set.default_permutation = default_permutation;
            std::set<std::string> permutation_ids;
            std::optional<std::set<std::string>> selection_keys;
            std::set<std::string> canonical_keys;
            int index = 0;
            for (auto const &permutation_value : permutations_value.arr) {
                auto context = "variant set " + set_name + " permutation " + std::to_string(index);
                require_object(permutation_value, context);
                check_keys(permutation_value, {"id", "select"}, context);
                auto permutation_id = identifier(
                    require_key_string(permutation_value, "id", context + " id"),
                    context + " id");
                if (permutation_ids.find(permutation_id) != permutation_ids.end()) {
                    throw ShaderVariantError("Duplicate permutation id " + permutation_id +
                                             " in variant set " + set_name);
                }
                permutation_ids.insert(permutation_id);
                auto const &selection_raw = require_key_object(permutation_value, "select",
                                                               context + " select");
                Permutation permutation;
                permutation.identifier = permutation_id;
                std::vector<std::pair<std::string, Json const *>> sorted_selection;
                for (auto const &entry : selection_raw.obj) {
                    sorted_selection.emplace_back(entry.first, &entry.second);
                }
                std::sort(sorted_selection.begin(), sorted_selection.end(), [](auto const &a, auto const &b) {
                    return a.first < b.first;
                });
                for (auto const &selection_entry : sorted_selection) {
                    auto dimension_name = identifier(Json::make_string(selection_entry.first), "selected dimension");
                    auto value_name = identifier(*selection_entry.second, "selected dimension value");
                    if (config.dimensions.find(dimension_name) == config.dimensions.end()) {
                        throw ShaderVariantError("Unknown dimension " + dimension_name + " in variant set " + set_name);
                    }
                    if (config.dimensions[dimension_name].values.find(value_name) ==
                        config.dimensions[dimension_name].values.end()) {
                        throw ShaderVariantError("Unknown value " + value_name + " for dimension " + dimension_name);
                    }
                    permutation.selection[dimension_name] = value_name;
                }
                if (permutation.selection.empty()) {
                    throw ShaderVariantError("Variant set " + set_name +
                                             " permutations must select a dimension");
                }
                std::set<std::string> current_keys;
                for (auto const &entry : permutation.selection) {
                    current_keys.insert(entry.first);
                }
                if (!selection_keys.has_value()) {
                    selection_keys = current_keys;
                } else if (current_keys != *selection_keys) {
                    throw ShaderVariantError("All permutations in variant set " + set_name +
                                             " must select the same dimensions");
                }
                auto key = canonical_selection(permutation.selection);
                if (canonical_keys.find(key) != canonical_keys.end()) {
                    throw ShaderVariantError("Duplicate selection " + key + " in variant set " + set_name);
                }
                canonical_keys.insert(key);
                variant_set.permutations.push_back(std::move(permutation));
                ++index;
            }
            if (permutation_ids.find(default_permutation) == permutation_ids.end()) {
                throw ShaderVariantError("Default permutation " + default_permutation +
                                         " is missing from variant set " + set_name);
            }
            auto default_iter = std::find_if(variant_set.permutations.begin(), variant_set.permutations.end(),
                                             [&](Permutation const &permutation) {
                                                 return permutation.identifier == default_permutation;
                                             });
            if (default_iter == variant_set.permutations.end()) {
                throw ShaderVariantError("Default permutation " + default_permutation +
                                         " is missing from variant set " + set_name);
            }
            for (auto const &entry : default_iter->selection) {
                auto dimension_iter = config.dimensions.find(entry.first);
                if (dimension_iter == config.dimensions.end()) {
                    throw ShaderVariantError("Unknown dimension " + entry.first);
                }
                if (dimension_iter->second.default_value != entry.second) {
                    throw ShaderVariantError(
                        "Default permutation of " + set_name + " must use the global default for dimension " +
                        entry.first);
                }
            }
            config.variant_sets[set_name] = std::move(variant_set);
        }
    }

    {
        auto programs_value = require_list(*raw.get("programs"), "programs");
        std::set<std::string> program_ids;
        int index = 0;
        for (auto const &program_value : programs_value.arr) {
            auto context = "program " + std::to_string(index);
            require_object(program_value, context);
            check_keys(program_value, {"id", "source", "variant_set", "host_abi", "defines"}, context);
            Program program;
            program.identifier = safe_relative(
                require_key_string(program_value, "id", context + " id"),
                context + " id");
            program.source = safe_relative(
                require_key_string(program_value, "source", "program " + program.identifier + " source"),
                "program " + program.identifier + " source");
            auto variant_set_name = identifier(
                require_key_string(program_value, "variant_set",
                                   "program " + program.identifier + " variant_set"),
                "program " + program.identifier + " variant_set");
            program.host_abi = require_key_string(program_value, "host_abi",
                                                  "program " + program.identifier + " host_abi");
            if (program.host_abi != "stable" && program.host_abi != "per_variant") {
                throw ShaderVariantError("Program " + program.identifier +
                                         " host_abi must be 'stable' or 'per_variant'");
            }
            if (config.variant_sets.find(variant_set_name) == config.variant_sets.end()) {
                throw ShaderVariantError("Program " + program.identifier +
                                         " references unknown variant set " + variant_set_name);
            }
            program.variant_set = variant_set_name;
            if (program_ids.find(program.identifier) != program_ids.end()) {
                throw ShaderVariantError("Duplicate program id: " + program.identifier);
            }
            program_ids.insert(program.identifier);

            auto source_path = std::filesystem::weakly_canonical(config.shader_root / program.source);
            auto source_dir = std::filesystem::weakly_canonical(config.shader_root / config.source_root);
            std::error_code ec;
            auto relative = std::filesystem::relative(source_path, source_dir, ec);
            if (ec || relative.empty() || relative.string().find("..") != std::string::npos) {
                throw ShaderVariantError("Program " + program.identifier +
                                         " source is outside compile.source_root");
            }
            if (relative.extension() != ".cpp") {
                throw ShaderVariantError("Program " + program.identifier + " source must be a .cpp file");
            }
            relative.replace_extension();
            program.source_identifier = path_to_posix(relative);
            if (!std::filesystem::is_regular_file(source_path, ec)) {
                throw ShaderVariantError("Program source does not exist: " + program.source);
            }
            Json const empty_object = Json::make_object({});
            auto defines_value = program_value.get("defines");
            program.defines = parse_defines(defines_value ? *defines_value : empty_object,
                                            "program " + program.identifier + " defines");
            config.programs.push_back(std::move(program));
            ++index;
        }
    }

    if (!std::filesystem::is_directory(config.source_dir())) {
        throw ShaderVariantError("Shader source root does not exist: " + config.source_dir().string());
    }
    for (auto const &include_dir : config.include_dirs) {
        auto include_path = config.shader_root / include_dir;
        if (!std::filesystem::is_directory(include_path)) {
            throw ShaderVariantError("Shader include directory does not exist: " + include_path.string());
        }
    }
    auto source_programs = config.source_files();
    if (source_programs.empty()) {
        throw ShaderVariantError("No shader .cpp files were found");
    }
    for (auto const &program : config.programs) {
        if (source_programs.find(program.source_identifier) == source_programs.end()) {
            throw ShaderVariantError(
                "Program source was not discovered below source_root: " + program.source);
        }
        if (source_programs.find(program.identifier) != source_programs.end() &&
            program.identifier != program.source_identifier) {
            throw ShaderVariantError(
                "Program id " + program.identifier + " collides with a different physical shader source");
        }
    }
    config.effective_defines({});
    for (auto const &variant_set_entry : config.variant_sets) {
        for (auto const &permutation : variant_set_entry.second.permutations) {
            config.effective_defines(permutation.selection);
        }
    }
    for (auto const &program : config.programs) {
        auto const &variant_set = config.variant_sets[program.variant_set];
        for (auto const &permutation : variant_set.permutations) {
            config.effective_program_defines(program, permutation.selection);
        }
    }
    std::set<std::string> used_scene_features;
    for (auto const &variant_set_entry : config.variant_sets) {
        auto const &variant_set_name = variant_set_entry.first;
        if (config.family_programs(variant_set_name).empty()) {
            throw ShaderVariantError("Variant set " + variant_set_name + " is not referenced by any program");
        }
        auto rules = config.variant_rules(variant_set_name);
        validate_variant_rules(variant_set_name, rules);
        for (auto const &rule : rules) {
            used_scene_features.insert(rule.required_features.begin(), rule.required_features.end());
            used_scene_features.insert(rule.forbidden_features.begin(), rule.forbidden_features.end());
        }
    }
    std::set<std::string> unused;
    for (auto const &feature : config.scene_features) {
        if (used_scene_features.find(feature) == used_scene_features.end()) {
            unused.insert(feature);
        }
    }
    if (!unused.empty()) {
        throw ShaderVariantError("Declared scene features are not used by any variant set: " +
                                 join_sorted(unused));
    }
    return config;
}

} // namespace

ShaderVariantConfig load_config(std::filesystem::path const &manifest_path) {
    return load_config_impl(manifest_path);
}

std::string tree_digest(ShaderVariantConfig const &config) {
    std::map<std::string, std::filesystem::path> files;
    files[path_to_posix(config.manifest_path.lexically_relative(config.shader_root))] =
        config.manifest_path;
    std::vector<std::filesystem::path> roots;
    roots.push_back(config.source_dir());
    for (auto const &include_dir : config.include_dirs) {
        roots.push_back(config.shader_root / include_dir);
    }
    std::error_code ec;
    for (auto const &root : roots) {
        if (!std::filesystem::is_directory(root, ec)) {
            continue;
        }
        for (auto const &entry : std::filesystem::recursive_directory_iterator(root, ec)) {
            if (entry.is_regular_file(ec)) {
                files[path_to_posix(entry.path().lexically_relative(config.shader_root))] = entry.path();
            }
        }
    }
    Sha256 digest;
    for (auto const &entry : files) {
        digest.update(entry.first);
        digest.update("\0");
        auto bytes = read_file_bytes(entry.second);
        digest.update(reinterpret_cast<std::uint8_t const *>(bytes.data()), bytes.size());
        digest.update("\0");
    }
    return digest.hex();
}

namespace {

std::vector<std::string> parse_includes(std::string const &content) {
    std::vector<std::string> result;
    std::size_t pos = 0;
    while ((pos = content.find("#include", pos)) != std::string::npos) {
        std::size_t p = pos + 8;
        while (p < content.size() && (content[p] == ' ' || content[p] == '\t')) {
            ++p;
        }
        if (p < content.size() && content[p] == '"') {
            auto end = content.find('"', p + 1);
            if (end != std::string::npos) {
                result.push_back(content.substr(p + 1, end - p - 1));
            }
        } else if (p < content.size() && content[p] == '<') {
            auto end = content.find('>', p + 1);
            if (end != std::string::npos) {
                result.push_back(content.substr(p + 1, end - p - 1));
            }
        }
        pos = p;
    }
    return result;
}

bool inside_shader_root(std::filesystem::path const &path,
                        std::filesystem::path const &shader_root) {
    auto relative = path.lexically_relative(shader_root);
    return !relative.empty() && relative.string().find("..") == std::string::npos;
}

} // namespace

std::string unit_digest(ShaderVariantConfig const &config,
                        std::filesystem::path const &source_path) {
    std::error_code ec;
    auto source = std::filesystem::weakly_canonical(source_path, ec);
    if (ec || !std::filesystem::is_regular_file(source, ec)) {
        throw ShaderVariantError("Unit digest source is not a file: " + source_path.string());
    }
    std::vector<std::filesystem::path> include_dirs;
    for (auto const &include_dir : config.include_dirs) {
        include_dirs.push_back(std::filesystem::weakly_canonical(config.shader_root / include_dir, ec));
    }
    std::map<std::string, std::filesystem::path> files;
    std::vector<std::filesystem::path> queue;
    queue.push_back(source);
    while (!queue.empty()) {
        auto current = queue.back();
        queue.pop_back();
        auto relative = current.lexically_relative(config.shader_root);
        auto key = path_to_posix(relative);
        if (files.find(key) != files.end()) {
            continue;
        }
        files[key] = current;
        auto content = read_file_text(current);
        for (auto const &include : parse_includes(content)) {
            bool angle = !include.empty() && include.front() == '<';
            (void)angle;
            // parse_includes already strips the delimiters; try quoted-style first
            // (relative to the including file), then the configured include dirs.
            std::filesystem::path resolved;
            bool found = false;
            auto try_base = [&](std::filesystem::path const &base) {
                auto candidate = base / include;
                if (std::filesystem::is_regular_file(candidate, ec)) {
                    resolved = std::filesystem::weakly_canonical(candidate, ec);
                    found = true;
                }
            };
            try_base(current.parent_path());
            if (!found) {
                for (auto const &dir : include_dirs) {
                    try_base(dir);
                    if (found) {
                        break;
                    }
                }
            }
            if (found && inside_shader_root(resolved, config.shader_root)) {
                queue.push_back(resolved);
            }
        }
    }
    Sha256 digest;
    for (auto const &entry : files) {
        digest.update(entry.first);
        digest.update("\0");
        auto bytes = read_file_bytes(entry.second);
        digest.update(reinterpret_cast<std::uint8_t const *>(bytes.data()), bytes.size());
        digest.update("\0");
    }
    return digest.hex();
}

ShaderVariantConfig snapshot_config(ShaderVariantConfig const &config,
                                    std::filesystem::path const &snapshot_root) {
    std::error_code ec;
    std::filesystem::create_directories(snapshot_root, ec);
    std::set<std::string> relative_roots;
    relative_roots.insert(config.source_root);
    relative_roots.insert(config.include_dirs.begin(), config.include_dirs.end());
    for (auto const &relative : relative_roots) {
        copy_tree(config.shader_root / relative, snapshot_root / relative);
    }
    auto manifest_relative = config.manifest_path.lexically_relative(config.shader_root);
    auto snapshot_manifest = snapshot_root / manifest_relative;
    std::filesystem::create_directories(snapshot_manifest.parent_path(), ec);
    std::filesystem::copy_file(config.manifest_path, snapshot_manifest,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        throw ShaderVariantError("Cannot snapshot manifest: " + ec.message());
    }
    return load_config(snapshot_manifest);
}

} // namespace rbc_shader
