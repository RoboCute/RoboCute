#pragma once
#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/types.h"

namespace rbc {

struct AnimSequenceResource : world::ResourceBaseImpl<AnimSequenceResource> {


public:
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

namespace rbc {

struct RBC_RUNTIME_API IAnimSequenceImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::AnimSequence; }
    virtual bool import(AnimSequenceResource *resource, luisa::filesystem::path const &path) = 0;

protected:
};

}// namespace rbc