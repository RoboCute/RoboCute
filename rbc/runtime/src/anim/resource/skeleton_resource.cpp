#include "rbc_anim/resource/skeleton_resource.h"
#include "rbc_world/type_register.h"

namespace rbc {

void SkeletonResource::serialize_meta(world::ObjSerialize const &ser) const {
    BaseType::serialize_meta(ser);// common attribute (type_id, file_path, etc)
    ser.ar.value(skeleton, "skeleton");
}

void SkeletonResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
    ser.ar.value(skeleton, "skeleton");
}

rbc::coroutine SkeletonResource::_async_load() {
    co_return;
}

bool SkeletonResource::unsafe_save_to_path() const {
    return {};// TODO
}

void SkeletonResource::_unload() {
}

void SkeletonResource::log_brief() {
    LUISA_INFO("skel has {} joints {} soa joints", skeleton.NumJoints(), skeleton.NumSOAJoints());
    const int NUM_LOG_JOINTS = skeleton.NumJoints() > 10 ? 10 : skeleton.NumJoints();
    for (int i = 0; i < NUM_LOG_JOINTS; i++) {
        auto &joint_name = skeleton.RawJointNames()[i];
        auto &parent = skeleton.RawJointParents()[i];
        LUISA_INFO("skel {} <{}> has parent {}", i, joint_name, parent);
        int soa_idx = i / 4;
        const ozz::math::SoaTransform soa_pose = skeleton.JointRestPoses()[soa_idx];
        float x[4];
        float y[4];
        float z[4];
        float w[4];

        ozz::math::StorePtrU(soa_pose.scale.x, x);
        ozz::math::StorePtrU(soa_pose.scale.y, y);
        ozz::math::StorePtrU(soa_pose.scale.z, z);
        int soa_offset = i % 4;
        ozz::math::Float3 scale{x[soa_offset], y[soa_offset], z[soa_offset]};
        LUISA_INFO("scale {} {} {}", scale.x, scale.y, scale.z);

        ozz::math::StorePtrU(soa_pose.translation.x, x);
        ozz::math::StorePtrU(soa_pose.translation.y, y);
        ozz::math::StorePtrU(soa_pose.translation.z, z);
        ozz::math::Float3 translation{x[soa_offset], y[soa_offset], z[soa_offset]};
        LUISA_INFO("translation {} {} {}", translation.x, translation.y, translation.z);

        ozz::math::StorePtrU(soa_pose.rotation.x, x);
        ozz::math::StorePtrU(soa_pose.rotation.y, y);
        ozz::math::StorePtrU(soa_pose.rotation.z, z);
        ozz::math::StorePtrU(soa_pose.rotation.w, w);
        ozz::math::Quaternion rot{x[soa_offset], y[soa_offset], z[soa_offset], w[soa_offset]};
        LUISA_INFO("rot {} {} {} {}", rot.x, rot.y, rot.z, rot.w);
    }
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkeletonResource)

}// namespace rbc

void rbc::Serialize<rbc::SkeletonResource>::write(rbc::ArchiveWrite &w, const rbc::SkeletonResource &v) {
    rbc::world::ObjSerialize ser_obj{w};
    v.serialize_meta(ser_obj);
}
bool rbc::Serialize<rbc::SkeletonResource>::read(rbc::ArchiveRead &r, rbc::SkeletonResource &v) {
    rbc::world::ObjDeSerialize deser_obj{r};
    v.deserialize_meta(deser_obj);
    return true;
}