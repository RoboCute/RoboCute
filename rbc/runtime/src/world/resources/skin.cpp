#include "rbc_world/resources/skin.h"
#include "rbc_world/type_register.h"

namespace rbc {

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
