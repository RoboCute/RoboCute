#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"

namespace rbc {

struct SkeletonResource : world::ResourceBaseImpl<SkeletonResource> {

    using BaseType = world::ResourceBaseImpl<SkeletonResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkeletonResource)
    void dispose() override;

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;
    luisa::string_view value() const;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;

    // depended resources
    luisa::vector<RC<SkeletonResource>> _depended;
    luisa::vector<char> _value;
};

}// namespace rbc
RBC_RTTI(rbc::SkeletonResource)

namespace rbc {

struct RBC_RUNTIME_API ISkeletonImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::Skeleton; }
    virtual bool import(SkeletonResource *resource, luisa::filesystem::path const &path) = 0;
protected:
};

}// namespace rbc