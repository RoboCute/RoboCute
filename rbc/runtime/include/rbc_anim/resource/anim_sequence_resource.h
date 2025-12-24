#pragma once
#include "rbc_world/resource_base.h"
#include "rbc_anim/types.h"

namespace rbc {

struct AnimSequenceResource : world::ResourceBaseImpl<AnimSequenceResource> {

    using BaseType = world::ResourceBaseImpl<AnimSequenceResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimSequenceResource)
    void dispose() override;

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coro::coroutine _async_load() override;

protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;
};

}// namespace rbc
RBC_RTTI(rbc::AnimSequenceResource)

// helper classes
namespace rbc {

// import animation resource from gltf
struct GLTFAnimImporter {
    luisa::string asset_path;
    luisa::string anim_name;
    float sampling_rate = 30.0f;

    void Import();//path -> RawAnimation
    void Cook();  // RawAnimation -> AnimSequenceResource
};

}// namespace rbc