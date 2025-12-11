#include <rbc_world_v2/material.h>
#include "type_register.h"
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/fiber.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_graphics/mat_serde.h>
#include <rbc_core/json_serde.h>
#include <rbc_world_v2/texture.h>
namespace rbc::world {
static luisa::spin_mutex _mat_mtx;
static luisa::vector<uint> _disposed_mat;
void _collect_all_materials() {
    auto sm = SceneManager::instance_ptr();
    _mat_mtx.lock();
    auto mats = std::move(_disposed_mat);
    _mat_mtx.unlock();
    if (!sm) {
        return;
    }
    for (auto &i : mats) {
        sm->mat_manager().discard_mat_instance(MatCode{i});
    }
}
struct MaterialImpl : Material {
    using BaseType = ResourceBaseImpl<Material>;
    luisa::fiber::event _event;
    luisa::vector<RC<Resource>> _depended_resources;
    MaterialImpl()
        : _event() {
    }
    bool loaded() const override {
        return _loaded;
    }
    void load_from_json(luisa::string_view json_vec) override {
        JsonDeSerializer deser{json_vec};
        luisa::string mat_type;
        if (!deser._load(mat_type, "type")) return;
        std::lock_guard lck{_mtx};
        if (mat_type == "pbr") {
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
            //TODO: other types
        }
    }
    luisa::BinaryBlob write_content_to() override {
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
            LUISA_ASSERT(iter == _depended_resources.end());
            _mat_data.visit([&]<typename T>(T const &t) {
                if constexpr (std::is_same_v<T, material::OpenPBR>) {
                    json_ser._store("type"sv, "pbr");
                    detail::serde_openpbr<material::OpenPBR const &>(t, ser_pbr);
                } else {
                    // TODO: serialize other type
                }
            });
        }
        return json_ser.write_to();
    }
    void rbc_objser(JsonSerializer &ser) const override {
        BaseType::rbc_objser(ser);
    }
    void rbc_objdeser(JsonDeSerializer &ser) override {
        BaseType::rbc_objdeser(ser);
    }
    bool async_load_from_file() override {
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
    ~MaterialImpl() {
        wait_load();
        unload();
    }
    void unload() override {
        std::lock_guard lck{_mtx};
        _depended_resources.clear();
        _loaded = false;
        if (_mat_code.value == ~0u) return;
        auto value = _mat_code.value;
        _mat_code.value = 0u;
        std::lock_guard lck1{_mat_mtx};
        _disposed_mat.emplace_back(value);
    }
    void wait_load() const override {
        _event.wait();
    }
    void update_material() override {
        wait_load();
        auto render_device = RenderDevice::instance_ptr();
        if (!render_device) return;
        std::lock_guard lck{_mtx};
        if (!loaded()) [[unlikely]] {
            LUISA_ERROR("Material not loaded yet.");
        }
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
    }
    void prepare_material() override {
        wait_load();
        auto iter = _depended_resources.begin();
        auto serde_func = [&]<typename U>(U &u, char const *name) {
            if constexpr (std::is_same_v<U, MatImageHandle>) {
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
        LUISA_ASSERT(iter == _depended_resources.end(), "Material type mismatch.");
        _mat_data.visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, material::OpenPBR>) {
                rbc::detail::serde_openpbr(
                    t,
                    serde_func);
            } else {
                //TODO
            }
        });
        auto render_device = RenderDevice::instance_ptr();
        if (!render_device) return;
        std::lock_guard lck{_mtx};
        if (!loaded()) [[unlikely]] {
            LUISA_ERROR("Material not loaded yet.");
        }
        if (_mat_code.value != ~0u)
            return;
        auto &sm = SceneManager::instance();
        _mat_data.visit([&]<typename T>(T &t) {
            _mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial, T>(
                t,
                render_device->lc_main_cmd_list(),
                sm.bindless_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue());
        });
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Material);
}// namespace rbc::world