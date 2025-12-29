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
#include "rbc_world/resource_importer.h"
#include "rbc_world/resources/skin.h"

namespace rbc {

struct RBC_RUNTIME_API SkelMeshResource : world::ResourceBaseImpl<SkelMeshResource> {

    using BaseType = world::ResourceBaseImpl<SkelMeshResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkelMeshResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;

    RC<SkinResource> GetSkinResource() const { return ref_skin; }

protected:
    bool unsafe_save_to_path() const override;

    RC<SkinResource> ref_skin;
};

}// namespace rbc
RBC_RTTI(rbc::SkelMeshResource)

namespace rbc {

struct RBC_RUNTIME_API ISkelMeshImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::SkelMesh; }
    virtual bool import(SkelMeshResource *resource, luisa::filesystem::path const &path) = 0;

protected:
};

}// namespace rbc