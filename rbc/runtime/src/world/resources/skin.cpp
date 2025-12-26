#include "rbc_world/resources/skin.h"
#include "rbc_world/type_register.h"

namespace rbc {

SkinResource::SkinResource() = default;
SkinResource::~SkinResource() {
    _unload();
}

void SkinResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
}

void SkinResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
}

rbc::coroutine SkinResource::_async_load() {
    co_return;
}

bool SkinResource::unsafe_save_to_path() const {
    return {};//TODO
}

void SkinResource::_unload() {
    // release ref
    ref_mesh.reset();
    ref_skel.reset();
}

void SkinResource::log_brief() {
    // SKR_LOG_FMT_INFO(u8"Skin has {} inverse bind poses and {} joint_remaps", inverse_bind_poses.size(), joint_remaps.size());
    LUISA_INFO("Skin has {} inverse bind poses and {} joint_remaps", inverse_bind_poses.size(), joint_remaps.size());
    auto log_brief = [](const AnimFloat4x4 &m) {
        std::array<std::array<float, 4>, 4> mat;
        for (size_t i = 0; i < 4; i++) {
            ozz::math::StorePtr(m.cols[i], mat[i].data());
        }
        LUISA_INFO("AnimFloat4x4:[");
        for (size_t i = 0; i < 4; i++) {
            LUISA_INFO("{} {} {} {}", mat[0][i], mat[1][i], mat[2][i], mat[3][i]);
        }
        LUISA_INFO("]");
    };
    if (inverse_bind_poses.size() > 0) {
        auto &inverse_bind_pose = inverse_bind_poses[0];
        log_brief(inverse_bind_pose);
    }
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkinResource)

// ISkinImporter
luisa::string &ISkinImporter::name_ref(SkinResource *resource) {
    return resource->name;
}
luisa::vector<luisa::string> &ISkinImporter::joint_remaps_ref(SkinResource *resource) {
    return resource->joint_remaps;
}
luisa::vector<AnimFloat4x4> &ISkinImporter::inverse_bind_poses_ref(SkinResource *resource) {
    return resource->inverse_bind_poses;
}
luisa::vector<BoneIndexType> &ISkinImporter::joint_remaps_LUT_ref(SkinResource *resource) {
    return resource->joint_remaps_LUT;
}
RC<SkeletonResource> &ISkinImporter::ref_skel_ref(SkinResource *resource) {
    return resource->ref_skel;
}
RC<world::MeshResource> &ISkinImporter::ref_mesh_ref(SkinResource *resource) {
    return resource->ref_mesh;
}

}// namespace rbc
