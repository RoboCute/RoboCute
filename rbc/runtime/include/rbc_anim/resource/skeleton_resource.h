#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/types.h"
#include "rbc_anim/asset/reference_skeleton.h"

namespace rbc {

struct SkeletonResource : world::ResourceBaseImpl<SkeletonResource> {

    using BaseType = world::ResourceBaseImpl<SkeletonResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkeletonResource)
    

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;
    luisa::string_view value() const;

public:
    const ReferenceSkeleton &ref_skel() const { return skeleton; }

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;

private:
    friend class ISkeletonImporter;
    ReferenceSkeleton skeleton;
};

}// namespace rbc
RBC_RTTI(rbc::SkeletonResource)

namespace rbc {

struct RBC_RUNTIME_API ISkeletonImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::Skeleton; }
    virtual bool import(SkeletonResource *resource, luisa::filesystem::path const &path) = 0;
protected:

    ReferenceSkeleton &ref_skel(SkeletonResource *resource) { return resource->skeleton; }
};

}// namespace rbc