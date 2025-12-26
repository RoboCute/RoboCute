#pragma once

#include "rbc_world/resource_base.h"
#include "rbc_world/resource_importer.h"
#include "rbc_anim/types.h"
#include "rbc_world/resources/skeleton.h"
#include "rbc_world/resources/mesh.h"

namespace rbc {

struct RBC_RUNTIME_API SkinResource : world::ResourceBaseImpl<SkinResource> {

    using BaseType = world::ResourceBaseImpl<SkinResource>;
    DECLARE_WORLD_OBJECT_FRIEND(SkinResource)

    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;

    rbc::coroutine _async_load() override;
    // meta data
    RC<SkeletonResource> ref_skel;
    RC<world::MeshResource> ref_mesh;
    void generate_LUT();
    
protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;



private:
    friend class ISkinImporter;
    // direct data
    luisa::string name;
    luisa::vector<luisa::string> joint_remaps;
    luisa::vector<AnimFloat4x4> inverse_bind_poses;
    // generated data
    luisa::vector<BoneIndexType> joint_remaps_LUT;// LUT: joint_remaps[i] -> bone index in skel

};

}// namespace rbc
RBC_RTTI(rbc::SkinResource)

namespace rbc {

struct RBC_RUNTIME_API ISkinImporter : world::IResourceImporter {
    [[nodiscard]] world::ResourceType resource_type() const override { return world::ResourceType::Skin; }
    virtual bool import(SkinResource *resource, luisa::filesystem::path const &path) = 0;
protected:
    static luisa::string &name_ref(SkinResource *resource);
    static luisa::vector<luisa::string> &joint_remaps_ref(SkinResource *resource);
    static luisa::vector<AnimFloat4x4> &inverse_bind_poses_ref(SkinResource *resource);
    static luisa::vector<BoneIndexType> &joint_remaps_LUT_ref(SkinResource *resource);
    static RC<SkeletonResource> &ref_skel_ref(SkinResource *resource);
    static RC<world::MeshResource> &ref_mesh_ref(SkinResource *resource);
};

}// namespace rbc