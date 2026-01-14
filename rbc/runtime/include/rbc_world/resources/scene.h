#pragma once

#include <rbc_world/resource_base.h>

namespace rbc::world {
struct Entity;
struct RBC_RUNTIME_API SceneResource : ResourceBaseImpl<SceneResource> {
    DECLARE_WORLD_OBJECT_FRIEND(SceneResource)
    rbc::coroutine _async_load() override;
    bool load_from_json(luisa::filesystem::path const &path);
    void update_data();
    Entity *get_entity(vstd::Guid guid);
    Entity *get_or_add_entity(vstd::Guid guid);
    // TODO: manage resources
private:
    bool unsafe_save_to_path() const;
    bool _install() override;
    SceneResource();
    ~SceneResource();
private:
    rbc::shared_atomic_mutex _map_mtx;
    vstd::HashMap<vstd::Guid, RC<Entity>> _entities;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::SceneResource)
