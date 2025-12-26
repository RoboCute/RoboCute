#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"

namespace rbc {

struct RBC_RUNTIME_API AnimGraphResource : world::ResourceBaseImpl<AnimGraphResource> {

    using BaseType = world::ResourceBaseImpl<AnimGraphResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimGraphResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;
};

}// namespace rbc
RBC_RTTI(rbc::AnimGraphResource)

namespace rbc {

struct RBC_RUNTIME_API IAnimGraphImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::AnimGraph; }
    virtual bool import(AnimGraphResource *resource, luisa::filesystem::path const &path) = 0;
protected:
};

}// namespace rbc