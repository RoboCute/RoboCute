#include <rbc_world/resources/material.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/fiber.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_graphics/mat_serde.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/resources/texture.h>
#include <rbc_core/runtime_static.h>

namespace rbc::world {
struct MaterialInst : RBCStruct {
    luisa::spin_mutex _mat_mtx;
    luisa::vector<uint> _disposed_mat;
    MatCode _default_mat_code{};
};
static RuntimeStatic<MaterialInst> _mat_inst;
MatCode MaterialResource::default_mat_code() {
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Renderer::update_object can only be called in render-thread.");
    }
    if (_mat_inst->_default_mat_code.value == ~0u) {
        material::OpenPBR mat{};
        auto &sm = SceneManager::instance();
        _mat_inst->_default_mat_code = sm.mat_manager().emplace_mat_instance(
            mat,
            RenderDevice::instance().lc_main_cmd_list(),
            sm.bindless_allocator(),
            sm.buffer_uploader(),
            sm.dispose_queue(),
            material::PolymorphicMaterial::index<material::OpenPBR>);
    }
    return _mat_inst->_default_mat_code;
}
void _collect_all_materials() {
    auto sm = SceneManager::instance_ptr();
    _mat_inst->_mat_mtx.lock();
    auto mats = std::move(_mat_inst->_disposed_mat);
    _mat_inst->_mat_mtx.unlock();
    if (!sm) {
        return;
    }
    for (auto &i : mats) {
        sm->mat_manager().discard_mat_instance(MatCode{i});
    }
}
void MaterialResource::load_from_json(luisa::string_view json_vec) {
    _load_from_json(json_vec, true);
}
void MaterialResource::_load_from_json(luisa::string_view json_vec, bool set_to_loaded) {
    _status = EResourceLoadingStatus::Loading;
    JsonDeSerializer deser{json_vec};
    luisa::string mat_type;
    bool is_default{false};
    if (!deser._load(mat_type, "type")) {
        is_default = true;
    }
    std::lock_guard lck{_async_mtx};
    _depended_resources.clear();
    _loaded = true;
    _dirty = true;
    if (is_default || mat_type == "pbr") {
        _mat_data.reset_as<material::OpenPBR>();
        auto serde_func = [&]<typename U>(U &u, char const *name) {
            using PureU = std::remove_cvref_t<U>;
            constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
            constexpr bool is_index = requires { u.index; };
            if constexpr (is_index) {
                vstd::Guid resource_guid;
                RC<Resource> res;
                auto set_res = vstd::scope_exit([&]() {
                    _depended_resources.emplace_back(std::move(res));
                });
                if (!deser._load(resource_guid, name)) return;
                res = load_resource(resource_guid, true);
                if (!res) {
                    return;
                }
            } else if constexpr (is_array) {
                uint64_t size;
                if (!deser.start_array(size, name))
                    return;
                LUISA_ASSERT(size == u.size(), "Variable {} array size mismatch.", name);
                for (auto &i : u) {
                    deser._load(i);
                }
                deser.end_scope();
            } else {
                deser._load(u, name);
            }
        };
        rbc::detail::serde_openpbr(
            _mat_data.force_get<material::OpenPBR>(),
            serde_func);
    } else {
        LUISA_ERROR("Unknown material type.");
        //TODO: other types
    }
    if (set_to_loaded) {
        _status = EResourceLoadingStatus::Loaded;
    }
}
luisa::BinaryBlob MaterialResource::write_content_to() {
    JsonSerializer json_ser;
    {
        std::lock_guard lck{_async_mtx};
        auto iter = _depended_resources.begin();
        auto ser_pbr = [&]<typename U>(U &u, char const *name) {
            using PureU = std::remove_cvref_t<U>;
            constexpr bool is_index = requires { u.index; };
            constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
            if constexpr (is_index) {
                LUISA_DEBUG_ASSERT(iter != _depended_resources.end());
                auto res = *iter;
                if (res && res->base_type() == BaseObjectType::Resource) {
                    auto guid = res->guid();
                    if (guid) {
                        json_ser._store(guid, name);
                    }
                }
                ++iter;
            } else if constexpr (is_array) {
                json_ser.start_array();
                for (auto &i : u) {
                    json_ser._store(i);
                }
                json_ser.add_last_scope_to_object(name);
            } else {
                json_ser._store(u, name);
            }
        };
        _mat_data.visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, material::OpenPBR>) {
                json_ser._store("type"sv, "pbr");
                rbc::detail::serde_openpbr(t, ser_pbr);
            } else {
                LUISA_ERROR("Unknown material type.");
                // TODO: serialize_meta other type
            }
        });
        LUISA_ASSERT(iter == _depended_resources.end(), "Material type mismatch.");
    }
    return json_ser.write_to();
}
void MaterialResource::serialize_meta(ObjSerialize const &ser) const {
    // TODO: mark dependencies
    // for (auto &i : _depended_resources) {
    //     auto guid = i->guid();
    //     if (guid)
    //         ser.depended_resources.emplace(guid);
    // }
}
bool MaterialResource::_async_load_from_file() {
    auto path = this->path();
    if (path.empty()) return false;
    if (_loaded) {
        return false;
    }
    _loaded = true;
    BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return false;
    luisa::vector<char> json_vec;
    json_vec.push_back_uninitialized(file_stream.length());
    file_stream.read(
        {reinterpret_cast<std::byte *>(json_vec.data()),
         json_vec.size()});
    _load_from_json({json_vec.data(), json_vec.size()}, false);
    return true;
}
MaterialResource::MaterialResource() {}

MaterialResource::~MaterialResource() {
    _depended_resources.clear();
    _loaded = false;
    if (_mat_code.value == ~0u) return;
    auto value = _mat_code.value;
    _mat_code.value = ~0u;
    if (_mat_inst) [[likely]] {
        std::lock_guard lck1{_mat_inst->_mat_mtx};
        _mat_inst->_disposed_mat.emplace_back(value);
    }
}
bool MaterialResource::_init_device_resource() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Material::init_device_resource can only be called in render-thread.");
    }
    std::lock_guard lck{_async_mtx};
    if (!_loaded) [[unlikely]] {
        return false;
    }
    if (!_dirty) return false;
    _dirty = false;
    auto iter = _depended_resources.begin();
    auto serde_func = [&]<typename U>(U &u, char const *name) {
        if constexpr (std::is_same_v<std::remove_cvref_t<U>, MatImageHandle>) {
            LUISA_DEBUG_ASSERT(iter != _depended_resources.end());
            auto res = *iter;
            if (res) {
                u.index = static_cast<TextureResource *>(res.get())->heap_index();
            } else {
                u.index = ~0u;
            }
            ++iter;
        }
    };
    _mat_data.visit([&]<typename T>(T &t) {
        if constexpr (std::is_same_v<T, material::OpenPBR>) {
            rbc::detail::serde_openpbr(
                t,
                serde_func);
        } else {
            LUISA_ERROR("Unknown material type.");
            //TODO
        }
    });
    LUISA_ASSERT(iter == _depended_resources.end(), "Material type mismatch.");

    auto &sm = SceneManager::instance();
    _mat_data.visit([&]<typename T>(T &t) {
        if (_mat_code.value == ~0u) {
            _mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial, T>(
                t,
                render_device->lc_main_cmd_list(),
                sm.bindless_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue());
        } else {
            sm.mat_manager().set_mat_instance(
                _mat_code,
                sm.buffer_uploader(),
                {(std::byte const *)&t,
                 sizeof(t)});
        }
    });
    return true;
}

rbc::coroutine MaterialResource::_async_load() {
    if (!_async_load_from_file()) {
        co_return;
    }
    for (auto &i : _depended_resources) {
        if (i)
            co_await i->await_loading();
    }
    _status = EResourceLoadingStatus::Loaded;
    co_return;
}

bool MaterialResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_mat_data.valid()) return false;
    JsonSerializer t;
    return _mat_data.visit_or(false, [&]<typename T>(T const &mat) {
        if constexpr (std::is_same_v<T, material::OpenPBR>) {
            t._store("type"sv, "pbr");
            auto iter = _depended_resources.begin();
            auto serde_func = [&]<typename U>(U &u, char const *name) {
                using PureU = std::remove_cvref_t<U>;
                constexpr bool is_index = requires { u.index; };
                constexpr bool is_array = requires {u.begin(); u.end(); u.data(); u.size(); };
                if constexpr (is_index) {
                    if (*iter) {
                        t._store((*iter)->guid(), name);
                    } else {
                        vstd::Guid guid;
                        guid.reset();
                        t._store(guid, name);
                    }
                    ++iter;
                } else if constexpr (is_array) {
                    t.start_array();
                    for (auto &i : u) {
                        t._store(i);
                    }
                    t.add_last_scope_to_object(name);
                } else {
                    t._store(u, name);
                }
            };
            rbc::detail::serde_openpbr(mat, serde_func);
            LUISA_ASSERT(iter == _depended_resources.end(), "Material type mismatch.");
            auto blob = t.write_to();
            if (blob.empty()) [[unlikely]]
                return false;
            BinaryFileWriter writer(luisa::to_string(path()));
            if (!writer._file) [[unlikely]]
                return false;
            writer.write({blob.data(),
                          blob.size()});
            return true;
        } else {
            // TODO
            return false;
        }
    });
}
// clang-format off
DECLARE_WORLD_OBJECT_REGISTER(MaterialResource);
// clang-format on
}// namespace rbc::world