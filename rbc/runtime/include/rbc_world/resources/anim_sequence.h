#pragma once
#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/asset/anim_sequence.h"
#include "rbc_world/resources/skeleton.h"
#include "rbc_anim/types.h"

namespace rbc {

struct RBC_RUNTIME_API AnimSequenceResource : world::ResourceBaseImpl<AnimSequenceResource> {


public:
    using BaseType = world::ResourceBaseImpl<AnimSequenceResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimSequenceResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    const AnimSequence &ref_seq() const { return anim_sequence; }
protected:
    bool unsafe_save_to_path() const override;

private:
    friend class IAnimSequenceImporter;
    AnimSequence anim_sequence;
    RC<SkeletonResource> ref_skeleton;
};

}// namespace rbc
RBC_RTTI(rbc::AnimSequenceResource)

namespace rbc {

struct RBC_RUNTIME_API IAnimSequenceImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::AnimSequence; }
    virtual bool import(AnimSequenceResource *resource, luisa::filesystem::path const &path) = 0;

protected:
    AnimSequence &seq_ref(AnimSequenceResource *resource) { return resource->anim_sequence; }
    RC<SkeletonResource> skel_ref(AnimSequenceResource *resource) { return resource->ref_skeleton; }
};

}// namespace rbc