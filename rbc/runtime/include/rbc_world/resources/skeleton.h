#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/types.h"
#include "rbc_anim/asset/reference_skeleton.h"

namespace rbc::world {

struct RBC_RUNTIME_API SkeletonResource : world::ResourceBaseImpl<SkeletonResource> {
    using BaseType = world::ResourceBaseImpl<SkeletonResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkeletonResource)

    rbc::coroutine _async_load() override;
    luisa::string_view value() const;

    mutable rbc::shared_atomic_mutex _async_mtx;

public:
    const ReferenceSkeleton &ref_skel() const { return skeleton; }
    ReferenceSkeleton &ref_skel() { return skeleton; }
    void log_brief();

protected:
    bool unsafe_save_to_path() const override;

private:
    friend class ISkeletonImporter;
    friend struct rbc::Serialize<rbc::world::SkeletonResource>;

    ReferenceSkeleton skeleton;
};

}// namespace rbc::world
RBC_RTTI(rbc::world::SkeletonResource)

namespace rbc::world {

struct RBC_RUNTIME_API ISkeletonImporter : world::IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override { return TypeInfo::get<SkeletonResource>().md5(); }
};

}// namespace rbc::world

template<>
struct rbc::Serialize<rbc::world::SkeletonResource> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::world::SkeletonResource &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::world::SkeletonResource &v);
};
