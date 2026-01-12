#pragma once

#include <rbc_world/resource_base.h>

namespace rbc::world {
struct Entity;
struct RBC_RUNTIME_API SceneResource : ResourceBaseImpl<SceneResource> {
    DECLARE_WORLD_OBJECT_FRIEND(SceneResource)
    rbc::coroutine _async_load() override;
    void enable();
    void update_data();
private:
    bool unsafe_save_to_path() const;
    SceneResource();
    ~SceneResource();
private:
    luisa::vector<RC<Entity>> _entities;
    std::atomic_bool _enabled{false};
};
}// namespace rbc::world
RBC_RTTI(rbc::world::SceneResource)
