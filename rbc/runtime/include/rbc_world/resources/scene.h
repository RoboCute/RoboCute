#pragma once

#include <rbc_world/resource_base.h>

namespace rbc::world {
struct SceneImporter;
struct Entity;
struct RBC_RUNTIME_API SceneResource : ResourceBaseImpl<SceneResource> {
    friend struct SceneImporter;
    friend struct Entity;
    DECLARE_WORLD_OBJECT_FRIEND(SceneResource)
    rbc::coroutine _async_load() override;
    void update_data();
    Entity *get_entity(vstd::Guid guid);
    Entity *get_or_add_entity(vstd::Guid guid);
    Entity *get_entity(luisa::string_view name);
    bool remove_entity(vstd::Guid guid);
    bool remove_entity(Entity *entity);
    luisa::span<Entity *const> get_entities(luisa::string_view name);
    // TODO: manage resources
private:
    bool load_from_json(luisa::filesystem::path const &path);
    bool unsafe_save_to_path() const;
    bool _install() override;
    void _set_entity_name(Entity *e, luisa::string const &new_name);
    SceneResource();
    ~SceneResource();
    rbc::shared_atomic_mutex _map_mtx;
    rbc::shared_atomic_mutex _name_map_mtx;
    vstd::HashMap<vstd::Guid, RC<Entity>> _entities;
    vstd::HashMap<luisa::string, luisa::fixed_vector<Entity *, 1>> _entities_str_name;
    vstd::optional<luisa::vector<char>> json_data;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::SceneResource)
