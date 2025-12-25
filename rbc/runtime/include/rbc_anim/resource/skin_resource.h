#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"

namespace rbc {

struct SkinResource : world::ResourceBaseImpl<SkinResource> {

    using BaseType = world::ResourceBaseImpl<SkinResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkinResource)
    

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;
};

}// namespace rbc
RBC_RTTI(rbc::SkinResource)

namespace rbc {

struct RBC_RUNTIME_API ISkinImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::Skin; }
    virtual bool import(SkinResource *resource, luisa::filesystem::path const &path) = 0;
protected:
};

}// namespace rbc