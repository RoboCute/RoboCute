#include <rbc_world_v2/material.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/fiber.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_graphics/mat_serde.h>
#include <rbc_core/json_serde.h>
#include <rbc_world_v2/texture.h>
namespace rbc::world {
struct MaterialInst : vstd::IOperatorNewBase {
    luisa::spin_mutex _mat_mtx;
    luisa::vector<uint> _disposed_mat;
};
static RuntimeStatic<MaterialInst> _mat_inst;

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
bool Material::loaded() const {
    return _loaded && _mat_code.value != ~0u;
}
void Material::load_from_json(luisa::string_view json_vec) {
    JsonDeSerializer deser{json_vec};
    luisa::string mat_type;
    bool is_default{false};
    if (!deser._load(mat_type, "type")) {
        is_default = true;
    }
    std::lock_guard lck{_mtx};
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
                BaseObject *res{};
                auto set_res = vstd::scope_exit([&]() {
                    _depended_resources.emplace_back(static_cast<Resource *>(res));
                });
                if (!deser._load(resource_guid, name)) return;
                res = get_object(resource_guid);
                if (!res || res->base_type() != BaseObjectType::Resource) {
                    return;
                }
                static_cast<Resource *>(res)->async_load_from_file();
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
}
luisa::BinaryBlob Material::write_content_to() {
    JsonSerializer json_ser;
    {
        std::lock_guard lck{_mtx};
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
                detail::serde_openpbr<material::OpenPBR const &>(t, ser_pbr);
            } else {
                LUISA_ERROR("Unknown material type.");
                // TODO: serialize other type
            }
        });
        LUISA_ASSERT(iter == _depended_resources.end(), "Material type mismatch.");
    }
    return json_ser.write_to();
}
void Material::serialize(ObjSerialize const &ser) const {
    BaseType::serialize(ser);
    // TODO: mark dependencies
    // for (auto &i : _depended_resources) {
    //     auto guid = i->guid();
    //     if (guid)
    //         ser.depended_resources.emplace(guid);
    // }
}
bool Material::async_load_from_file() {
    if (_path.empty()) return false;
    std::lock_guard lck{_mtx};
    if (_loaded) {
        return false;
    }
    _loaded = true;
    _event.clear();
    luisa::fiber::schedule([evt = _event, this]() {
        evt.signal();
        BinaryFileStream file_stream(luisa::to_string(_path));
        if (!file_stream.valid()) return;
        if (_file_offset >= file_stream.length()) return;
        file_stream.set_pos(_file_offset);
        luisa::vector<char> json_vec;
        json_vec.push_back_uninitialized(file_stream.length() - _file_offset);
        file_stream.read(
            {reinterpret_cast<std::byte *>(json_vec.data()),
             json_vec.size()});
        load_from_json({json_vec.data(), json_vec.size()});
    });
    return true;
}
Material::Material()
    : _event(luisa::fiber::event::Mode::Manual, true) {}
Material::~Material() {
    wait_load();
    unload();
}
void Material::unload() {
    std::lock_guard lck{_mtx};
    _depended_resources.clear();
    _loaded = false;
    if (_mat_code.value == ~0u) return;
    auto value = _mat_code.value;
    _mat_code.value = ~0u;
    std::lock_guard lck1{_mat_inst->_mat_mtx};
    _mat_inst->_disposed_mat.emplace_back(value);
}
void Material::wait_load() const {
    _event.wait();
}
bool Material::init_device_resource() {
    std::lock_guard lck{_mtx};
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("Material::init_device_resource can only be called in render-thread.");
    }
    wait_load();
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
                res->wait_load();
                u.index = static_cast<Texture *>(res.get())->heap_index();
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
// clang-format off
DECLARE_WORLD_OBJECT_REGISTER(Material);
// clang-format on
}// namespace rbc::world