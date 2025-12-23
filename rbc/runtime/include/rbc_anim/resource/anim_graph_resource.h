#pragma once

#include "rbc_world/resource_base.h"

namespace rbc {

struct AnimGraphResource : world::ResourceBaseImpl<AnimGraphResource> {

    using BaseType = world::ResourceBaseImpl<AnimGraphResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimGraphResource)
    void dispose() override;

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;
};

}// namespace rbc
RBC_RTTI(rbc::AnimGraphResource)
