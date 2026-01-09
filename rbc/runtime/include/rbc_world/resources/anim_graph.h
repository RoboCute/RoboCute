#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/graph/AnimGraph.h"

namespace rbc {

struct RBC_RUNTIME_API AnimGraphResource : world::ResourceBaseImpl<AnimGraphResource> {

    using BaseType = world::ResourceBaseImpl<AnimGraphResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimGraphResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    AnimGraph graph;

protected:
    bool unsafe_save_to_path() const override;
};

}// namespace rbc
RBC_RTTI(rbc::AnimGraphResource)

namespace rbc {

struct RBC_RUNTIME_API IAnimGraphImporter : world::IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override { return TypeInfo::get<AnimGraphResource>().md5(); }
    virtual bool import(AnimGraphResource *resource, luisa::filesystem::path const &path) = 0;
protected:
};

}// namespace rbc