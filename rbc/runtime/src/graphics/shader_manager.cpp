#include <rbc_graphics/shader_manager.h>
#include <luisa/core/stl/algorithm.h>
#include <luisa/core/logging.h>
#include <yyjson.h>

namespace rbc {

// singleton
static ShaderManager *_shader_manager_instance{};
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
}
ShaderManager::~ShaderManager() {}
ShaderBase ShaderManager::unload_shader(
    luisa::filesystem::path const &shader_path) {
    luisa::string key_str;
    luisa::string can_path_str;
    auto key_strview = _path_to_key(shader_path, can_path_str, key_str);
    _mtx.lock();
    auto iter = _shaders.find(key_strview);
    if (!iter) {
        _mtx.unlock();
        return ShaderBase{};
    }
    auto &value = iter.value();
    value._evt.wait();
    _shaders.remove(iter);
    _mtx.unlock();
    LUISA_DEBUG_ASSERT(value.shader.is_type_of<ShaderBase>());
    ShaderBase v = std::move(value.shader.force_get<ShaderBase>());
    value.shader.dispose();
    return v;
}
luisa::string_view ShaderManager::_path_to_key(luisa::filesystem::path const &path, luisa::string &can_path_str, luisa::string &buffer) const {
    luisa::string_view key_strview;
    buffer = luisa::to_string(path);
    key_strview = buffer;
    auto shader_path = _shader_path / path;
    std::error_code ec;
    auto can_path = std::filesystem::weakly_canonical(shader_path, ec);
    if (ec) [[unlikely]] {
        LUISA_ERROR("Invalid file path {} error code: {}", luisa::to_string(shader_path), ec.message());
    }
    can_path_str = luisa::to_string(can_path);
    return key_strview;
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
    auto iter = _shaders.try_emplace(key_strview);
    auto &value = iter.first.value();
    _mtx.unlock();
    // , vstd::lazy_eval([&]() {
    // 	LUISA_WARNING("Shader {} not preloaded.", key_strview);
    // 	vstd::vector<Type const*> types_vec;

    // 	return ShaderVariant{, std::move(types_vec), reload_func};
    // })
    if (!iter.second)
        value._evt.wait();
    if (!support_preload)
        value.support_preload = false;
    bool is_equal = true;
    if (iter.second || value.arg_types.empty()) {
        luisa::visit(
            [&]<typename T>(T const &t) {
                if constexpr (std::is_same_v<typename T::value_type, Variable>) {
                    vstd::push_back_func(value.arg_types, t.size(), [&](size_t i) { return t[i].type(); });
                } else {
                    vstd::push_back_all(value.arg_types, t);
                }
            },
            args);
    } else {
        is_equal = luisa::visit([&]<typename T>(T const &t) {
            if (value.arg_types.size() != t.size()) return false;
            for (auto i : vstd::range(value.arg_types.size())) {
                if constexpr (std::is_same_v<typename T::value_type, Variable>) {
                    if (value.arg_types[i] != t[i].type()) return false;
                } else {
                    if (value.arg_types[i] != t[i]) return false;
                }
            }
            return true;
        },
                                args);
    }
    if (!is_equal) [[unlikely]] {
        LUISA_ERROR("Load same shader {} multiple-times with different type.", can_path_str);
    }
    _all_shader_count++;
    if (!value.shader.valid())
        value.shader = create_func(can_path_str);
    _finished_shaders++;
    if (!value.shader.valid()) {
        _mtx.lock();
        _shaders.remove(key_strview);
        _mtx.unlock();
        return nullptr;
    }
    if (!value.reload_func)
        value.reload_func = std::move(reload_func);
    value._evt.signal();
    return &value.shader;
}
vstd::unique_ptr<vstd::IRange<string_view>> ShaderManager::loaded_shaders() const {
    auto iter = vstd::range_linker{
        vstd::make_ite_range(_shaders),
        vstd::transform_range{[&](auto &&kv) {
            return luisa::string_view{kv.first};
        }}};
    auto irange = std::move(iter).i_range();
    return vstd::make_unique<decltype(irange)>(std::move(irange));
}
vstd::vector<char> ShaderManager::loaded_shaders_json(luisa::filesystem::path const &shader_path) const {
    vstd::vector<char> vec;
    vec.reserve(1024);
    vec.push_back('{');
    bool first = true;
    for (auto &&i : _shaders) {
        if (!i.second.support_preload)
            continue;
        if (!first) [[likely]] {
            vec.push_back(',');
        } else {
            first = false;
        }
        vec.push_back('"');
        for (auto c : i.first) {
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
        for (auto &&arg : i.second.arg_types) {
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
void ShaderManager::get_preload_progress(uint64_t &all_shader_count, uint64_t &finished_shader_count) {
    all_shader_count = _all_shader_count;
    finished_shader_count = _finished_shaders;
}
void ShaderManager::preload_shaders(
    Device &device,
    luisa::filesystem::path const &shader_path,
    vstd::vector<std::pair<vstd::string, vstd::vector<Type const *>>> &&registed_shaders) {
    auto size = registed_shaders.size();
    for (auto &i : registed_shaders) {
        _shaders.try_emplace(i.first);
    }
    _all_shader_count = size;
    _finished_shaders = 0;
    fiber::async_parallel(size, [this, &device, shader_path, registed_shaders = std::move(registed_shaders)](size_t i) mutable {
        auto &js = registed_shaders[i];
        luisa::string &path_str = js.first;
        _mtx.lock();
        auto iter = _shaders.find(path_str);
        _mtx.unlock();
        LUISA_ASSERT(iter);
        auto &&arg_types = js.second;
        auto &v = iter.value();
        auto dsp = vstd::scope_exit([&, evt = v._evt] {
            _finished_shaders++;
            evt.signal();
        });
        auto key_strview = std::move(path_str);
        std::filesystem::path path = std::filesystem::weakly_canonical(shader_path / key_strview);
        path_str = luisa::to_string(path);
        if (!std::filesystem::exists(path)) {
            return;
        }
        auto dev_impl = device.impl();
        luisa::span<const Type *const> arg_types_span = arg_types;
        auto load_result = dev_impl->load_shader(path_str, arg_types_span);
        if (!load_result.valid()) {
            return;
        }
        ShaderBase shader_base{
            dev_impl,
            load_result,
            ShaderDispatchCmdEncoder::compute_uniform_size(arg_types_span)};
        v.shader = std::move(shader_base);
        v.arg_types = std::move(std::move(arg_types));
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
                    for (auto i : vstd::range(size)) {
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
struct ReloadLogic {
    using Map = vstd::HashMap<string, ShaderManager::ShaderVariant>;
    Device &device;
    spin_mutex _mtx;
    Map::Iterator iter;
    std::filesystem::path const *parent_path;
    ReloadLogic(
        Device &device,
        Map::Iterator &&iter,
        std::filesystem::path const *parent_path)
        : device(device), iter(std::move(iter)), parent_path(std::move(parent_path)) {
    }
    ReloadLogic(ReloadLogic &&rhs)
        : device(rhs.device), iter(std::move(rhs.iter)), parent_path(std::move(rhs.parent_path)) {
    }
    void operator()(uint i) {
        string const *key;
        ShaderManager::ShaderVariant *value;
        {
            std::lock_guard lck{_mtx};
            key = &iter->first;
            value = &iter->second;
            iter++;
        }
        if (value->reload_func == nullptr)
            return;
        if (!parent_path)
            value->shader = value->reload_func(device, *key);
        else
            value->shader = value->reload_func(device, luisa::to_string(*parent_path / *key));
    }
};
fiber::counter ShaderManager::reload_shaders(
    Device &device) {
    std::lock_guard lck{this->_mtx};
    if (_shaders.empty()) return {};
    return fiber::async_parallel(_shaders.size(), ReloadLogic{device, _shaders.begin(), &_shader_path});
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