#pragma once
#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/asset/anim_sequence.h"
#include "rbc_world/resources/skeleton.h"
#include "rbc_anim/types.h"

namespace rbc::world {

struct RBC_RUNTIME_API AnimSequenceResource : world::ResourceBaseImpl<AnimSequenceResource> {

public:
    using BaseType = world::ResourceBaseImpl<AnimSequenceResource>;
    DECLARE_WORLD_OBJECT_FRIEND(AnimSequenceResource)

    AnimSequenceResource();
    ~AnimSequenceResource();

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    const AnimSequence &ref_seq() const { return anim_sequence; }
    AnimSequence &ref_seq() { return anim_sequence; }
    RC<SkeletonResource> ref_skel;
    void log_brief();

    // Import config (stored in meta, set before importing)
    luisa::string anim_name;// empty = auto-select first animation
    float sampling_rate = 30.0f;

protected:
    bool unsafe_save_to_path() const override;

private:
    mutable rbc::shared_atomic_mutex _async_mtx;
    friend struct IAnimSequenceImporter;
    friend struct rbc::Serialize<AnimSequenceResource>;
    AnimSequence anim_sequence;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::AnimSequenceResource)

namespace rbc::world {

struct RBC_RUNTIME_API IAnimSequenceImporter : world::IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override { return TypeInfo::get<AnimSequenceResource>().md5(); }

protected:
};

}// namespace rbc::world

template<>
struct RBC_RUNTIME_API rbc::Serialize<rbc::world::AnimSequenceResource> {
    static bool write(rbc::ArchiveWrite &w, const rbc::world::AnimSequenceResource &v);
    static bool read(rbc::ArchiveRead &r, rbc::world::AnimSequenceResource &v);
};