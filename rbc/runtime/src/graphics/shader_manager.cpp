#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/shader_features.h>
#include <luisa/core/stl/algorithm.h>
#include <luisa/core/logging.h>
#include <yyjson.h>
#include <fstream>

namespace rbc {

// singleton
static ShaderManager *_shader_manager_instance{};

namespace {

bool parse_selection(yyjson_val *value, ShaderManager::VariantSelection &selection) {
    if (!value || unsafe_yyjson_get_type(value) != YYJSON_TYPE_OBJ) {
        return false;
    }
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(value, &iter);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&iter))) {
        auto item = yyjson_obj_iter_get_val(key);
        if (!item || unsafe_yyjson_get_type(item) != YYJSON_TYPE_STR) {
            return false;
        }
        selection.set(unsafe_yyjson_get_str(key), unsafe_yyjson_get_str(item));
    }
    return true;
}

bool parse_feature_mask(yyjson_val *value, uint64_t &mask) {
    mask = 0u;
    if (!value || unsafe_yyjson_get_type(value) != YYJSON_TYPE_ARR) {
        return false;
    }
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(value, &iter);
    yyjson_val *item;
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (unsafe_yyjson_get_type(item) != YYJSON_TYPE_STR) {
            return false;
        }
        auto feature = scene_shader_feature_mask(
            std::string_view{unsafe_yyjson_get_str(item)});
        if (!feature || (mask & *feature) != 0u) {
            return false;
        }
        mask |= *feature;
    }
    return true;
}

bool is_safe_artifact_path(luisa::filesystem::path const &path) {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (auto const &part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

bool is_sha256_hex(luisa::string_view value) noexcept {
    if (value.size() != 64u) {
        return false;
    }
    for (auto const c : value) {
        auto const decimal = c >= '0' && c <= '9';
        auto const lower_hex = c >= 'a' && c <= 'f';
        auto const upper_hex = c >= 'A' && c <= 'F';
        if (!decimal && !lower_hex && !upper_hex) {
            return false;
        }
    }
    return true;
}

bool path_is_within_root(
    luisa::filesystem::path const &path,
    luisa::filesystem::path const &root,
    luisa::filesystem::path &relative) {
    relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (auto const &part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

luisa::string shader_display_path(
    luisa::string_view canonical_name,
    luisa::filesystem::path const &shader_root) {
    std::error_code ec;
    auto const canonical_root = std::filesystem::weakly_canonical(shader_root, ec);
    if (ec) {
        return luisa::string{canonical_name};
    }
    luisa::filesystem::path relative;
    auto const canonical_path = luisa::filesystem::path{canonical_name};
    if (path_is_within_root(canonical_path, canonical_root, relative)) {
        auto const result = relative.generic_string();
        return luisa::string{result.data(), result.size()};
    }
    auto const result = canonical_path.generic_string();
    return luisa::string{result.data(), result.size()};
}

}// namespace

luisa::string ShaderManager::_canonical_logical_name(luisa::string_view name) {
    luisa::string result{name};
    for (auto &c : result) {
        if (c == '\\') {
            c = '/';
        }
    }
    while (result.starts_with("./")) {
        result.erase(0u, 2u);
    }
    if (result.ends_with(".bin")) {
        result.erase(result.size() - 4u);
    }
    return result;
}

ShaderManager *ShaderManager::instance() {
    if (_shader_manager_instance) {
        return _shader_manager_instance;
    } else {
        LUISA_ERROR("ShaderManager instance not created.");
        return nullptr;
    }
}
void ShaderManager::create_instance(Device &device, luisa::filesystem::path const &shader_path) {
    if (_shader_manager_instance) {
        LUISA_WARNING("ShaderManager instance already created.");
        return;
    }
    _shader_manager_instance = new ShaderManager(device, shader_path);
}
void ShaderManager::destroy_instance() {
    if (!_shader_manager_instance) {
        LUISA_WARNING("ShaderManager instance not created.");
        return;
    }
    delete _shader_manager_instance;
    _shader_manager_instance = nullptr;
}

ShaderManager::ShaderManager(Device &device, luisa::filesystem::path const &shader_path)
    : _device(device), _shader_path(shader_path) {
    _load_variant_manifest();
}
ShaderManager::~ShaderManager() {
    _preload_counter.wait();
}

void ShaderManager::_load_variant_manifest() {
    auto manifest_path = _shader_path / "shader_manifest.json";
    _manifest_present = luisa::filesystem::exists(manifest_path);
    if (!_manifest_present) {
        return;
    }

    std::ifstream file{manifest_path, std::ios::binary | std::ios::ate};
    if (!file) {
        LUISA_WARNING("Failed to open shader manifest {}.", luisa::to_string(manifest_path));
        return;
    }
    auto file_size = file.tellg();
    if (file_size <= 0) {
        LUISA_WARNING("Shader manifest {} is empty.", luisa::to_string(manifest_path));
        return;
    }
    vstd::vector<char> json(static_cast<size_t>(file_size));
    file.seekg(0, std::ios::beg);
    if (!file.read(json.data(), static_cast<std::streamsize>(json.size()))) {
        LUISA_WARNING("Failed to read shader manifest {}.", luisa::to_string(manifest_path));
        return;
    }

    yyjson_alc alc{
        .malloc = +[](void *, size_t size) { return vengine_malloc(size); },
        .realloc = +[](void *, void *ptr, size_t, size_t size) { return vengine_realloc(ptr, size); },
        .free = +[](void *, void *ptr) { vengine_free(ptr); }};
    yyjson_read_err read_error{};
    auto document = yyjson_read_opts(json.data(), json.size(), 0, &alc, &read_error);
    if (!document) {
        LUISA_WARNING(
            "Failed to parse shader manifest {} at byte {}: {}.",
            luisa::to_string(manifest_path),
            read_error.pos,
            read_error.msg ? read_error.msg : "unknown error");
        return;
    }
    auto release_document = vstd::scope_exit([document] { yyjson_doc_free(document); });
    auto root = yyjson_doc_get_root(document);
    if (!root || unsafe_yyjson_get_type(root) != YYJSON_TYPE_OBJ) {
        LUISA_WARNING("Shader manifest {} root must be an object.", luisa::to_string(manifest_path));
        return;
    }

    auto schema = yyjson_obj_get(root, "schema_version");
    auto backend = yyjson_obj_get(root, "backend");
    auto build_id = yyjson_obj_get(root, "build_id");
    auto input_id = yyjson_obj_get(root, "input_id");
    auto programs = yyjson_obj_get(root, "programs");
    auto families = yyjson_obj_get(root, "families");
    if (!schema || !yyjson_is_uint(schema) || yyjson_get_uint(schema) != 1u ||
        !backend || unsafe_yyjson_get_type(backend) != YYJSON_TYPE_STR ||
        !build_id || unsafe_yyjson_get_type(build_id) != YYJSON_TYPE_STR ||
        !input_id || unsafe_yyjson_get_type(input_id) != YYJSON_TYPE_STR ||
        !is_sha256_hex(unsafe_yyjson_get_str(input_id)) ||
        !programs || unsafe_yyjson_get_type(programs) != YYJSON_TYPE_OBJ ||
        !families || unsafe_yyjson_get_type(families) != YYJSON_TYPE_OBJ) {
        LUISA_WARNING(
            "Shader manifest {} has an unsupported schema or is missing required fields.",
            luisa::to_string(manifest_path));
        return;
    }

    luisa::string parsed_backend{unsafe_yyjson_get_str(backend)};
    if (parsed_backend != _device.backend_name()) {
        LUISA_WARNING(
            "Shader manifest backend {} does not match device backend {}.",
            parsed_backend,
            _device.backend_name());
        return;
    }

    vstd::HashMap<string, ManifestProgram> parsed_programs;
    yyjson_obj_iter program_iter;
    yyjson_obj_iter_init(programs, &program_iter);
    yyjson_val *program_key;
    while ((program_key = yyjson_obj_iter_next(&program_iter))) {
        auto program_value = yyjson_obj_iter_get_val(program_key);
        if (!program_value || unsafe_yyjson_get_type(program_value) != YYJSON_TYPE_OBJ) {
            LUISA_WARNING("Shader manifest program {} must be an object.", unsafe_yyjson_get_str(program_key));
            return;
        }
        ManifestProgram program;
        auto default_selection = yyjson_obj_get(program_value, "default_selection");
        if (!parse_selection(default_selection, program.default_selection)) {
            LUISA_WARNING("Shader manifest program {} has an invalid default_selection.", unsafe_yyjson_get_str(program_key));
            return;
        }
        auto variants = yyjson_obj_get(program_value, "variants");
        if (!variants || unsafe_yyjson_get_type(variants) != YYJSON_TYPE_ARR) {
            LUISA_WARNING("Shader manifest program {} has no variants array.", unsafe_yyjson_get_str(program_key));
            return;
        }
        yyjson_arr_iter variant_iter;
        yyjson_arr_iter_init(variants, &variant_iter);
        yyjson_val *variant_value;
        while ((variant_value = yyjson_arr_iter_next(&variant_iter))) {
            if (unsafe_yyjson_get_type(variant_value) != YYJSON_TYPE_OBJ) {
                LUISA_WARNING("Shader manifest program {} contains a non-object variant.", unsafe_yyjson_get_str(program_key));
                return;
            }
            ManifestVariant variant;
            variant.selection = program.default_selection;
            VariantSelection explicit_selection;
            if (!parse_selection(yyjson_obj_get(variant_value, "selection"), explicit_selection)) {
                LUISA_WARNING("Shader manifest program {} contains an invalid variant selection.", unsafe_yyjson_get_str(program_key));
                return;
            }
            for (auto const &item : explicit_selection.values) {
                variant.selection.set(item.first, item.second);
            }
            auto artifact = yyjson_obj_get(variant_value, "artifact");
            if (!artifact || unsafe_yyjson_get_type(artifact) != YYJSON_TYPE_STR) {
                LUISA_WARNING("Shader manifest program {} contains a variant without an artifact.", unsafe_yyjson_get_str(program_key));
                return;
            }
            variant.artifact = luisa::filesystem::path{unsafe_yyjson_get_str(artifact)};
            if (!is_safe_artifact_path(variant.artifact)) {
                LUISA_WARNING(
                    "Shader manifest program {} contains unsafe artifact path {}.",
                    unsafe_yyjson_get_str(program_key),
                    luisa::to_string(variant.artifact));
                return;
            }
            auto size = yyjson_obj_get(variant_value, "size");
            auto sha256 = yyjson_obj_get(variant_value, "sha256");
            if (!size || !yyjson_is_uint(size) || yyjson_get_uint(size) == 0u ||
                !sha256 || unsafe_yyjson_get_type(sha256) != YYJSON_TYPE_STR ||
                !is_sha256_hex(unsafe_yyjson_get_str(sha256))) {
                LUISA_WARNING(
                    "Shader manifest program {} contains invalid artifact integrity metadata.",
                    unsafe_yyjson_get_str(program_key));
                return;
            }
            variant.size = yyjson_get_uint(size);
            for (auto const &existing : program.variants) {
                if (existing.selection == variant.selection) {
                    LUISA_WARNING("Shader manifest program {} contains duplicate variant selections.", unsafe_yyjson_get_str(program_key));
                    return;
                }
            }
            program.variants.emplace_back(std::move(variant));
        }
        if (program.variants.empty()) {
            LUISA_WARNING("Shader manifest program {} contains no variants.", unsafe_yyjson_get_str(program_key));
            return;
        }
        auto logical_name = _canonical_logical_name(unsafe_yyjson_get_str(program_key));
        auto inserted = parsed_programs.try_emplace(logical_name, std::move(program));
        if (!inserted.second) {
            LUISA_WARNING("Shader manifest contains duplicate logical program {}.", logical_name);
            return;
        }
    }

    vstd::HashMap<string, ManifestFamily> parsed_families;
    yyjson_obj_iter family_iter;
    yyjson_obj_iter_init(families, &family_iter);
    yyjson_val *family_key;
    while ((family_key = yyjson_obj_iter_next(&family_iter))) {
        auto family_value = yyjson_obj_iter_get_val(family_key);
        if (!family_value ||
            unsafe_yyjson_get_type(family_value) != YYJSON_TYPE_OBJ) {
            LUISA_WARNING(
                "Shader manifest family {} must be an object.",
                unsafe_yyjson_get_str(family_key));
            return;
        }
        ManifestFamily family;
        auto family_programs = yyjson_obj_get(family_value, "programs");
        auto family_rules = yyjson_obj_get(family_value, "rules");
        if (!family_programs ||
            unsafe_yyjson_get_type(family_programs) != YYJSON_TYPE_ARR ||
            !family_rules ||
            unsafe_yyjson_get_type(family_rules) != YYJSON_TYPE_ARR) {
            LUISA_WARNING(
                "Shader manifest family {} is missing programs or rules.",
                unsafe_yyjson_get_str(family_key));
            return;
        }

        yyjson_arr_iter member_iter;
        yyjson_arr_iter_init(family_programs, &member_iter);
        yyjson_val *member_value;
        while ((member_value = yyjson_arr_iter_next(&member_iter))) {
            if (unsafe_yyjson_get_type(member_value) != YYJSON_TYPE_STR) {
                LUISA_WARNING(
                    "Shader manifest family {} contains an invalid program id.",
                    unsafe_yyjson_get_str(family_key));
                return;
            }
            auto logical_name = _canonical_logical_name(
                unsafe_yyjson_get_str(member_value));
            if (!parsed_programs.find(logical_name)) {
                LUISA_WARNING(
                    "Shader manifest family {} references missing program {}.",
                    unsafe_yyjson_get_str(family_key),
                    logical_name);
                return;
            }
            for (auto const &existing : family.programs) {
                if (existing == logical_name) {
                    LUISA_WARNING(
                        "Shader manifest family {} contains duplicate program {}.",
                        unsafe_yyjson_get_str(family_key),
                        logical_name);
                    return;
                }
            }
            family.programs.emplace_back(std::move(logical_name));
        }
        if (family.programs.empty()) {
            LUISA_WARNING(
                "Shader manifest family {} contains no programs.",
                unsafe_yyjson_get_str(family_key));
            return;
        }

        yyjson_arr_iter rule_iter;
        yyjson_arr_iter_init(family_rules, &rule_iter);
        yyjson_val *rule_value;
        while ((rule_value = yyjson_arr_iter_next(&rule_iter))) {
            if (unsafe_yyjson_get_type(rule_value) != YYJSON_TYPE_OBJ) {
                LUISA_WARNING(
                    "Shader manifest family {} contains an invalid selection rule.",
                    unsafe_yyjson_get_str(family_key));
                return;
            }
            ManifestFamilyRule rule;
            if (!parse_selection(
                    yyjson_obj_get(rule_value, "selection"),
                    rule.selection) ||
                !parse_feature_mask(
                    yyjson_obj_get(rule_value, "required_features"),
                    rule.required_features) ||
                !parse_feature_mask(
                    yyjson_obj_get(rule_value, "forbidden_features"),
                    rule.forbidden_features) ||
                (rule.required_features & rule.forbidden_features) != 0u) {
                LUISA_WARNING(
                    "Shader manifest family {} contains an invalid scene-feature rule.",
                    unsafe_yyjson_get_str(family_key));
                return;
            }
            for (auto const &existing : family.rules) {
                if (existing.selection == rule.selection) {
                    LUISA_WARNING(
                        "Shader manifest family {} contains duplicate selections.",
                        unsafe_yyjson_get_str(family_key));
                    return;
                }
            }
            for (auto const &logical_name : family.programs) {
                auto program_iter = parsed_programs.find(logical_name);
                auto const &program = program_iter.value();
                auto found = false;
                for (auto const &variant : program.variants) {
                    if (variant.selection == rule.selection) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LUISA_WARNING(
                        "Shader manifest family {} program {} is missing a declared selection.",
                        unsafe_yyjson_get_str(family_key),
                        logical_name);
                    return;
                }
            }
            family.rules.emplace_back(std::move(rule));
        }
        if (family.rules.empty()) {
            LUISA_WARNING(
                "Shader manifest family {} contains no selection rules.",
                unsafe_yyjson_get_str(family_key));
            return;
        }
        luisa::string family_name{unsafe_yyjson_get_str(family_key)};
        auto inserted = parsed_families.try_emplace(
            family_name, std::move(family));
        if (!inserted.second) {
            LUISA_WARNING(
                "Shader manifest contains duplicate family {}.",
                family_name);
            return;
        }
    }

    _manifest_backend = std::move(parsed_backend);
    _manifest_build_id = unsafe_yyjson_get_str(build_id);
    _manifest_programs = std::move(parsed_programs);
    _manifest_families = std::move(parsed_families);
    _manifest_valid = true;
}

bool ShaderManager::resolve_shader_variant(
    luisa::string_view logical_name,
    VariantSelection const &requested,
    VariantResolution &resolution) const {
    resolution = {};
    auto canonical_name = _canonical_logical_name(logical_name);
    if (_manifest_present) {
        if (!_manifest_valid) {
            LUISA_WARNING("Shader manifest is present but invalid; refusing to resolve {}.", canonical_name);
            return false;
        }
        auto program_iter = _manifest_programs.find(canonical_name);
        if (!program_iter) {
            LUISA_WARNING("Shader manifest does not contain logical program {}.", canonical_name);
            return false;
        }
        auto const &program = program_iter.value();
        auto const &complete_selection =
            requested.empty() ? program.default_selection : requested;
        auto find_variant = [&](VariantSelection const &selection) -> ManifestVariant const * {
            for (auto const &variant : program.variants) {
                if (variant.selection == selection) {
                    return &variant;
                }
            }
            return nullptr;
        };
        auto variant = find_variant(complete_selection);
        if (!variant) {
            LUISA_WARNING("Shader program {} has no exact requested variant.", canonical_name);
            return false;
        }
        auto artifact_path = _shader_path / variant->artifact;
        std::error_code size_error;
        auto const artifact_size = luisa::filesystem::file_size(
            artifact_path, size_error);
        if (size_error || artifact_size != variant->size) {
            LUISA_WARNING(
                "Shader variant artifact {} for {} is missing or has an unexpected size "
                "(expected {}, actual {}).",
                luisa::to_string(artifact_path),
                canonical_name,
                variant->size,
                size_error ? 0u : artifact_size);
            return false;
        }
        resolution.logical_name = canonical_name;
        resolution.artifact = variant->artifact;
        resolution.selection = variant->selection;
        resolution.build_id = _manifest_build_id;
        resolution.manifest_backed = true;
        return true;
    }

    if (!requested.empty()) {
        LUISA_WARNING(
            "Shader manifest is unavailable; non-default variant {} cannot be resolved.",
            canonical_name);
        return false;
    }
    auto artifact = luisa::filesystem::path{canonical_name + ".bin"};
    if (!luisa::filesystem::exists(_shader_path / artifact)) {
        LUISA_WARNING("Default shader artifact {} does not exist.", luisa::to_string(_shader_path / artifact));
        return false;
    }
    resolution.logical_name = canonical_name;
    resolution.artifact = std::move(artifact);
    resolution.build_id = "legacy-default";
    return true;
}

bool ShaderManager::select_shader_family_variant(
    luisa::string_view family_name,
    uint64_t scene_feature_mask,
    VariantSelection &selection) const {
    selection = {};
    if (!_manifest_valid) {
        LUISA_WARNING(
            "A valid shader manifest is required to select family {}.",
            family_name);
        return false;
    }
    auto family_iter = _manifest_families.find(family_name);
    if (!family_iter) {
        LUISA_WARNING("Shader manifest does not contain family {}.", family_name);
        return false;
    }
    ManifestFamilyRule const *match{};
    for (auto const &rule : family_iter.value().rules) {
        auto const required =
            (scene_feature_mask & rule.required_features) ==
            rule.required_features;
        auto const forbidden =
            (scene_feature_mask & rule.forbidden_features) != 0u;
        if (required && !forbidden) {
            if (match) {
                LUISA_WARNING(
                    "Shader family {} has ambiguous scene-feature selection rules.",
                    family_name);
                return false;
            }
            match = &rule;
        }
    }
    if (!match) {
        LUISA_WARNING(
            "Shader family {} has no selection rule for scene feature mask {}.",
            family_name,
            scene_feature_mask);
        return false;
    }
    selection = match->selection;
    return true;
}

bool ShaderManager::resolve_shader_family(
    luisa::string_view family_name,
    VariantSelection const &selection,
    VariantFamilyResolution &resolution) const {
    resolution = {};
    if (!_manifest_valid) {
        LUISA_WARNING(
            "A valid shader manifest is required to resolve family {}.",
            family_name);
        return false;
    }
    auto family_iter = _manifest_families.find(family_name);
    if (!family_iter) {
        LUISA_WARNING("Shader manifest does not contain family {}.", family_name);
        return false;
    }

    VariantFamilyResolution candidate;
    candidate.selection = selection;
    candidate.build_id = _manifest_build_id;
    candidate.programs.reserve(family_iter.value().programs.size());
    for (auto const &logical_name : family_iter.value().programs) {
        VariantResolution program;
        if (!resolve_shader_variant(logical_name, selection, program)) {
            return false;
        }
        if (!(candidate.selection == program.selection) ||
            candidate.build_id != program.build_id) {
            LUISA_WARNING(
                "Shader bundle member {} resolved to a different selection or build generation.",
                logical_name);
            return false;
        }
        candidate.programs.emplace_back(std::move(program));
    }
    resolution = std::move(candidate);
    return true;
}
ShaderBase ShaderManager::unload_shader(
    luisa::filesystem::path const &shader_path) {
    if (_is_family_shader_path(shader_path)) {
        LUISA_WARNING(
            "Shader family artifact {} cannot be unloaded independently.",
            luisa::to_string(shader_path));
        return ShaderBase{};
    }
    luisa::string key_str;
    luisa::string can_path_str;
    auto key_strview = _path_to_key(shader_path, can_path_str, key_str);
    _mtx.lock();
    auto iter = _shaders.find(key_strview);
    if (!iter) {
        _mtx.unlock();
        return ShaderBase{};
    }
    auto value = iter.value();
    if (value.use_count() != 2u || !value->_evt.is_signalled()) {
        _mtx.unlock();
        LUISA_WARNING(
            "Shader {} cannot be unloaded while a cache operation is in flight.",
            key_strview);
        return ShaderBase{};
    }
    _shaders.remove(iter);
    _mtx.unlock();
    std::lock_guard entry_lock{value->local_mtx};
    LUISA_DEBUG_ASSERT(value->shader.is_type_of<ShaderBase>());
    ShaderBase v = std::move(value->shader.force_get<ShaderBase>());
    value->shader.dispose();
    return v;
}

bool ShaderManager::_is_family_shader_path(
    luisa::filesystem::path const &path) const {
    luisa::string candidate_path;
    luisa::string candidate_key;
    auto const candidate = _path_to_key(
        path, candidate_path, candidate_key);
    for (auto const &[family_name, family] : _manifest_families) {
        (void)family_name;
        for (auto const &logical_name : family.programs) {
            auto program = _manifest_programs.find(logical_name);
            if (!program) continue;
            for (auto const &variant : program.value().variants) {
                luisa::string artifact_path;
                luisa::string artifact_key;
                auto const artifact = _path_to_key(
                    variant.artifact, artifact_path, artifact_key);
                if (artifact == candidate) {
                    return true;
                }
            }
        }
    }
    return false;
}

luisa::string_view ShaderManager::_path_to_key(luisa::filesystem::path const &path, luisa::string &can_path_str, luisa::string &buffer) const {
    auto shader_path = path.is_absolute() ? path : _shader_path / path;
    std::error_code ec;
    auto can_path = std::filesystem::weakly_canonical(shader_path, ec);
    if (ec) [[unlikely]] {
        LUISA_ERROR("Invalid file path {} error code: {}", luisa::to_string(shader_path), ec.message());
    }
    can_path_str = luisa::to_string(can_path);
    buffer = can_path_str;
    return buffer;
}

auto ShaderManager::_load_shader(
    luisa::filesystem::path const &rela_shader_path,
    luisa::variant<
        luisa::span<Type const *const>,
        luisa::span<Variable const>>
        args,
    vstd::FuncRef<ShaderType(string_view shader_path)> &&create_func,
    ReloadFunc reload_func,
    bool support_preload) -> ShaderType const * {
    luisa::string key_str;
    luisa::string can_path_str;
    auto key_strview = _path_to_key(rela_shader_path, can_path_str, key_str);
    _mtx.lock();
    auto iter = _shaders.try_emplace(
        key_strview,
        vstd::lazy_eval([] { return luisa::make_shared<ShaderCacheEntry>(); }));
    auto value = iter.first.value();
    _mtx.unlock();
    if (!iter.second)
        value->_evt.wait();
    std::lock_guard entry_lock{value->local_mtx};
    if (!support_preload)
        value->support_preload = false;
    bool is_equal = true;
    if (value->arg_types.empty()) {
        luisa::visit(
            [&]<typename T>(T const &t) {
                if constexpr (std::is_same_v<typename T::value_type, Variable>) {
                    vstd::push_back_func(value->arg_types, t.size(), [&](size_t i) { return t[i].type(); });
                } else {
                    vstd::push_back_all(value->arg_types, t);
                }
            },
            args);
    } else {
        is_equal = luisa::visit([&]<typename T>(T const &t) {
            if (value->arg_types.size() != t.size()) return false;
            for (auto i : vstd::range(value->arg_types.size())) {
                if constexpr (std::is_same_v<typename T::value_type, Variable>) {
                    if (value->arg_types[i] != t[i].type()) return false;
                } else {
                    if (value->arg_types[i] != t[i]) return false;
                }
            }
            return true;
        },
                                args);
    }
    if (!is_equal) [[unlikely]] {
        LUISA_ERROR("Load same shader {} multiple-times with different type.", can_path_str);
    }
    if (!value->reload_func)
        value->reload_func = std::move(reload_func);
    if (!value->shader.valid()) {
        value->_evt.clear();
        _all_shader_count++;
        value->shader = create_func(can_path_str);
        _finished_shaders++;
    }
    value->_evt.signal();
    if (!value->shader.valid()) {
        return nullptr;
    }
    return &value->shader;
}
vstd::unique_ptr<vstd::IRange<string_view>> ShaderManager::loaded_shaders() const {
    vstd::vector<luisa::string> names;
    {
        std::lock_guard lock{_mtx};
        names.reserve(_shaders.size());
        for (auto const &[name, entry] : _shaders) {
            (void)entry;
            names.emplace_back(shader_display_path(name, _shader_path));
        }
    }
    auto iter = vstd::range_linker{
        vstd::make_ite_range(std::move(names)),
        vstd::transform_range{[](auto const &name) {
            return luisa::string_view{name};
        }}};
    auto irange = std::move(iter).i_range();
    return vstd::make_unique<decltype(irange)>(std::move(irange));
}
vstd::vector<char> ShaderManager::loaded_shaders_json(luisa::filesystem::path const &shader_path) const {
    using SnapshotItem = std::pair<
        luisa::string,
        luisa::shared_ptr<ShaderCacheEntry>>;
    vstd::vector<SnapshotItem> shaders;
    {
        std::lock_guard lock{_mtx};
        shaders.reserve(_shaders.size());
        for (auto const &[name, entry] : _shaders) {
            shaders.emplace_back(name, entry);
        }
    }
    vstd::vector<char> vec;
    vec.reserve(1024);
    vec.push_back('{');
    bool first = true;
    for (auto const &[canonical_name, entry_ptr] : shaders) {
        std::lock_guard entry_lock{entry_ptr->local_mtx};
        auto const &entry = *entry_ptr;
        if (!entry.support_preload)
            continue;
        if (!first) [[likely]] {
            vec.push_back(',');
        } else {
            first = false;
        }
        auto const name = shader_display_path(canonical_name, shader_path);
        vec.push_back('"');
        for (auto c : name) {
            if (c == '\\') [[unlikely]] {
                vec.push_back('/');
            } else {
                vec.push_back(c);
            }
        }
        vec.push_back('"');
        vec.push_back(':');
        vec.push_back('[');
        bool local_first = true;
        for (auto &&arg : entry.arg_types) {
            if (!local_first) [[likely]] {
                vec.push_back(',');
            } else {
                local_first = false;
            }
            vec.push_back('"');
            auto desc = arg->description();
            vstd::push_back_all(vec, span<char const>{desc.data(), desc.size()});
            vec.push_back('"');
        }
        vec.push_back(']');
    }
    vec.push_back('}');
    return vec;
}
void ShaderManager::get_preload_progress(uint64_t &all_shader_count, uint64_t &finished_shader_count) const {
    all_shader_count = _all_shader_count;
    finished_shader_count = _finished_shaders;
}
void ShaderManager::preload_shaders(
    Device &device,
    luisa::filesystem::path const &shader_path,
    vstd::vector<std::pair<vstd::string, vstd::vector<Type const *>>> &&registed_shaders) {
    auto size = registed_shaders.size();
    for (auto &i : registed_shaders) {
        auto preload_path = luisa::filesystem::path{i.first};
        if (preload_path.is_relative()) {
            preload_path = shader_path / preload_path;
        }
        std::error_code path_error;
        preload_path = std::filesystem::weakly_canonical(
            preload_path, path_error);
        if (path_error) [[unlikely]] {
            LUISA_ERROR(
                "Invalid preload shader path {} error code: {}",
                luisa::to_string(preload_path),
                path_error.message());
        }
        luisa::string canonical_path;
        luisa::string key_buffer;
        (void)_path_to_key(preload_path, canonical_path, key_buffer);
        i.first = std::move(key_buffer);
        _mtx.lock();
        _shaders.try_emplace(
            i.first,
            vstd::lazy_eval([] { return luisa::make_shared<ShaderCacheEntry>(); }));
        _mtx.unlock();
    }
    _all_shader_count = size;
    _finished_shaders = 0;
    _preload_counter.wait();
    fiber::async_parallel(_preload_counter, size, [this, &device, registed_shaders = std::move(registed_shaders)](size_t i) mutable {
        auto &js = registed_shaders[i];
        auto const &path_str = js.first;
        luisa::string canonical_path;
        luisa::string key_buffer;
        auto const key = _path_to_key(path_str, canonical_path, key_buffer);
        _mtx.lock();
        auto iter = _shaders.find(key);
        auto v = iter ? iter.value() : nullptr;
        _mtx.unlock();
        LUISA_ASSERT(v);
        auto &&arg_types = js.second;
        std::lock_guard entry_lock{v->local_mtx};
        auto dsp = vstd::scope_exit([&, evt = v->_evt] {
            _finished_shaders++;
            evt.signal();
        });
        if (v->shader.valid()) {
            return;
        }
        v->_evt.clear();
        std::filesystem::path path{canonical_path};
        if (!std::filesystem::exists(path)) {
            return;
        }
        auto dev_impl = device.impl();
        luisa::span<const Type *const> arg_types_span = arg_types;
        auto load_result = dev_impl->load_shader(canonical_path, arg_types_span);
        if (!load_result.valid()) {
            return;
        }
        ShaderBase shader_base{
            dev_impl,
            load_result,
            ShaderDispatchCmdEncoder::compute_uniform_size(arg_types_span)};
        v->shader = std::move(shader_base);
        v->arg_types = std::move(arg_types);
    });
}
void ShaderManager::preload_shaders(
    Device &device,
    luisa::filesystem::path const &shader_path, vstd::string_view json_str) {
    yyjson_alc alc{
        .malloc = +[](void *, size_t size) { return vengine_malloc(size); }, .realloc = +[](void *, void *ptr, size_t old_size, size_t size) { return vengine_realloc(ptr, size); }, .free = +[](void *, void *ptr) { vengine_free(ptr); }};
    auto json_doc = yyjson_read_opts(const_cast<char *>(json_str.data()), json_str.size(), 0, &alc, NULL);
    vstd::vector<std::pair<vstd::string, vstd::vector<Type const *>>> arr;
    if (json_doc) {
        auto root_val = json_doc->root;
        if (root_val) {
            if (unsafe_yyjson_get_type(root_val) == YYJSON_TYPE_OBJ) {
                yyjson_obj_iter iter;
                yyjson_obj_iter_init(root_val, &iter);
                yyjson_val *key, *val;
                while ((key = yyjson_obj_iter_next(&iter))) {
                    val = yyjson_obj_iter_get_val(key);
                    if (unsafe_yyjson_get_type(val) != YYJSON_TYPE_ARR) continue;
                    auto &ele = arr.emplace_back();
                    ele.first = unsafe_yyjson_get_str(key);
                    auto size = unsafe_yyjson_get_len(val);
                    ele.second.reserve(size);
                    auto node = unsafe_yyjson_get_first(val);
                    for (auto idx [[maybe_unused]] : vstd::range(size)) {
                        if (unsafe_yyjson_get_type(node) != YYJSON_TYPE_STR) continue;
                        ele.second.emplace_back(Type::from(unsafe_yyjson_get_str(node)));
                        node = unsafe_yyjson_get_next(node);
                    }
                }
            }
        }
        // _json_deser_func(self, func_table, ctx, ptr, json_doc->root);
        yyjson_doc_free(json_doc);
    }
    preload_shaders(device, shader_path, std::move(arr));
}
ShaderBase const *ShaderManager::load_typeless(luisa::filesystem::path const &shader_path, luisa::span<Type const *const> types, bool support_preload) {
    auto c1 = [&](string_view shader_path) -> ShaderType {
        auto uniform_size = ShaderDispatchCmdEncoder::compute_uniform_size(types);
        return ShaderBase{
            _device.impl(),
            _device.impl()->load_shader(shader_path, types),
            uniform_size};
    };
    auto c2 = [](Device &device, string_view name, luisa::span<Type const *const> arg_types) -> ShaderType {
        auto uniform_size = ShaderDispatchCmdEncoder::compute_uniform_size(arg_types);
        return ShaderBase{
            device.impl(),
            device.impl()->load_shader(name, arg_types),
            uniform_size};
    };

    auto shader = _load_shader(
        shader_path,
        types,
        c1, c2,
        support_preload);
    return shader ? shader->template try_get<ShaderBase>() : nullptr;
}

fiber::counter ShaderManager::reload_shaders(Device &device) {
    using ReloadItem = std::pair<
        luisa::string,
        luisa::shared_ptr<ShaderCacheEntry>>;
    vstd::vector<ReloadItem> shaders;
    {
        std::lock_guard lock{_mtx};
        shaders.reserve(_shaders.size());
        for (auto const &[path, entry] : _shaders) {
            if (!_is_family_shader_path(path)) {
                shaders.emplace_back(path, entry);
            }
        }
    }
    if (shaders.empty()) return {};
    return fiber::async_parallel(
        shaders.size(),
        [&device, shaders = std::move(shaders)](size_t i) mutable {
            auto &[path, entry] = shaders[i];
            entry->_evt.wait();
            std::lock_guard entry_lock{entry->local_mtx};
            if (!entry->reload_func) return;
            entry->_evt.clear();
            auto signal = vstd::scope_exit([event = entry->_evt] {
                event.signal();
            });
            auto reloaded = entry->reload_func(
                device, path, entry->arg_types);
            if (reloaded.valid()) {
                entry->shader = std::move(reloaded);
            }
        });
}
void ShaderManager::_empty_path_error() {
    LUISA_ERROR("ShaderOption::name can not be empty.");
}
void ShaderManager::_captured_not_empty_error(luisa::string_view name) {
    LUISA_ERROR("Shader {} has captured runtime resources, can not use AOT compilation.", name);
}
void shader_notfound_log(string_view name) {
    LUISA_ERROR("Shader {} not found.", name);
}
}// namespace rbc
