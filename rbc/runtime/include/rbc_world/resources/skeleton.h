#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/types.h"
#include "rbc_anim/asset/reference_skeleton.h"

namespace rbc {

struct RBC_RUNTIME_API SkeletonResource : world::ResourceBaseImpl<SkeletonResource> {

    using BaseType = world::ResourceBaseImpl<SkeletonResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkeletonResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

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

template<>
struct rbc::Serialize<rbc::SkeletonResource> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::SkeletonResource &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::SkeletonResource &v);
};