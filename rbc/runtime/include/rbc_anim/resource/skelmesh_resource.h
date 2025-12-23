#pragma once
/**
 * The Skelmesh Resource
 * ============================================
 * Represent a Runnable SkeletonMesh, a combination of 
 * * Skeleton
 * * Mesh
 * * Skin
 * * AnimGraph
 * * * ... A Sequence of AnimSequence
 */

#include "rbc_world/resource_base.h"

namespace rbc {

struct SkelmeshResource : world::ResourceBaseImpl<SkelmeshResource> {

    using BaseType = world::ResourceBaseImpl<SkelmeshResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkelmeshResource)
    void dispose() override;

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;
};

}// namespace rbc
RBC_RTTI(rbc::SkelmeshResource)
