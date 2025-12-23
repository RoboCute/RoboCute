#pragma once
#include "rbc_world_v2/resource_base.h"
namespace rbc {

struct RBC_APP_API SkeletonResource final : world::ResourceBaseImpl<SkeletonResource> {
    DECLARE_WORLD_OBJECT_FRIEND(SkeletonResource);
    using BaseType = ResourceBaseImpl<SkeletonResource>;
private:
    SkeletonResource();
    ~SkeletonResource();

public:
    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;
    void dispose() override;
};

}// namespace rbc

RBC_RTTI(rbc::SkeletonResource)