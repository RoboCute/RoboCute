#include <rbc_world/resources/scene.h>
#include <rbc_world/type_register.h>
#include <rbc_world/entity.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {
using namespace luisa;
Entity *SceneResource::get_entity(vstd::Guid guid) {
    std::shared_lock lck{_map_mtx};
    auto iter = _entities.find(guid);
    return iter ? iter.value().get() : nullptr;
}
Entity *SceneResource::get_or_add_entity(vstd::Guid guid) {
    std::shared_lock lck{_map_mtx};
    auto iter = _entities.try_emplace(
        guid,
        vstd::lazy_eval([&] {
            auto e = create_object_with_guid<Entity>(guid);
            e->_parent_scene = this;
            return e;
        }));
    return iter.first.value().get();
}
Entity *SceneResource::get_entity(luisa::string_view name) {
    std::shared_lock lck{_name_map_mtx};
    auto iter = _entities_str_name.find(name);
    if (!iter) return nullptr;
    auto &vec = iter.value();
    if (!vec.empty()) {
        return vec[0];
    }
    return nullptr;
}
luisa::span<Entity *const> SceneResource::get_entities(luisa::string_view name) {
    std::shared_lock lck{_name_map_mtx};
    auto iter = _entities_str_name.find(name);
    if (!iter) return {};
    auto &vec = iter.value();
    return vec;
}
bool SceneResource::load_from_json(luisa::filesystem::path const &path) {
    std::lock_guard lck{_map_mtx};
    LUISA_ASSERT(_entities.empty());// scene must be empty
    luisa::vector<char> json_vec;
    {
        BinaryFileStream file_stream(luisa::to_string(path));
        if (!file_stream.valid()) return false;
        json_vec.push_back_uninitialized(file_stream.length());
        file_stream.read(
            {reinterpret_cast<std::byte *>(json_vec.data()),
             json_vec.size()});
    }
    JsonDeSerializer deser{luisa::string_view{json_vec.data(), json_vec.size()}};
    LUISA_ASSERT(deser.valid());
    ArchiveReadJson read_adapter(deser);
    uint64_t size = deser.last_array_size();
    _entities.reserve(size);
    for (auto i : vstd::range(size / 2)) {
        vstd::Guid guid;
        LUISA_ASSERT(deser._load(guid));
        auto e =
            _entities
                .try_emplace(
                    guid, vstd::lazy_eval([&] {
                        auto e = create_object_with_guid<Entity>(guid);
                        e->_parent_scene = this;
                        return e;
                    }))
                .first.value();
        read_adapter.start_object();
        e->deserialize_meta(world::ObjDeSerialize{read_adapter});
        if (!e->_name.empty()) {
            std::lock_guard lck{_name_map_mtx};
            auto &vec = _entities_str_name.emplace(e->_name).value();
            e->_name_idx = vec.size();
            vec.emplace_back(e.get());
        }
        read_adapter.end_scope();
    }
    unsafe_set_loaded();
    return true;
}
rbc::coroutine SceneResource::_async_load() {
    auto path = this->path();
    if (path.empty()) co_return;
    load_from_json(path);
    co_return;
}
SceneResource::SceneResource() {}
SceneResource::~SceneResource() {
}
bool SceneResource::remove_entity(vstd::Guid guid) {
    RC<Entity> v;
    {
        std::lock_guard lck{_map_mtx};
        auto iter = _entities.find(guid);
        if (!iter) return false;
        v = std::move(iter.value());
        _entities.remove(iter);
    }
    v->_parent_scene = nullptr;
    v->_name_idx = ~0ull;
    _set_entity_name(v.get(), {});
    return true;
}
bool SceneResource::remove_entity(Entity *entity) {
    if (entity->_parent_scene != this) return false;
    return remove_entity(entity->guid());
}
bool SceneResource::unsafe_save_to_path() const {
    // already installed
    JsonSerializer ser{true};
    ArchiveWriteJson adapter(ser);
    for (auto &i : _entities) {
        ser._store(i.first);
        adapter.start_object();
        i.second->serialize_meta(world::ObjSerialize{adapter});
        adapter.end_object();
    }
    BinaryFileWriter file_writer(luisa::to_string(path()));
    file_writer.write(ser.write_to());
    return true;
}

void SceneResource::update_data() {
    for (auto &i : _entities) {
        i.second->unsafe_call_update();
    }
}

bool SceneResource::_install() {
    // init
    for (auto &i : _entities) {
        i.second->unsafe_call_awake();
    }
    update_data();
    return true;
}
void SceneResource::_set_entity_name(Entity *e, luisa::string const &new_name) {
    std::lock_guard lck{_name_map_mtx};
    if (new_name == e->_name) return;
    auto old_name = e->_name;
    if (!old_name.empty() && e->_name_idx != ~0ull) {
        auto iter = _entities_str_name.find(old_name);
        if (iter) {
            auto &vec = iter.value();
            auto &v = vec[e->_name_idx];
            LUISA_DEBUG_ASSERT(v == e);
            if (e->_name_idx < vec.size() - 1) {
                v = std::move(vec.back());
                v->_name_idx = e->_name_idx;
            }
            vec.pop_back();
            e->_name_idx = ~0ull;
        }
    }
    if (!new_name.empty()) {
        auto &vec = _entities_str_name.emplace(new_name).value();
        e->_name_idx = vec.size();
        vec.emplace_back(e);
    }
}

DECLARE_WORLD_OBJECT_REGISTER(SceneResource)
}// namespace rbc::world