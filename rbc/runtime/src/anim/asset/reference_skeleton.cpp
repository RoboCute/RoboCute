/**
 * Raw Skeleton Asset
 * Wraps the IO of ozz assets
 */

#include <rbc_anim/asset/reference_skeleton.h>
#include <rbc_anim/asset/ozz_stream.h>
#include <ozz/base/io/archive.h>

namespace rbc {
ReferenceSkeleton::ReferenceSkeleton() = default;
ReferenceSkeleton::~ReferenceSkeleton() {
}
ReferenceSkeleton::ReferenceSkeleton(ReferenceSkeleton &&other) noexcept {
    _skeleton = std::move(other._skeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(ReferenceSkeleton &&other) noexcept {
    _skeleton = std::move(other._skeleton);
    return *this;
}
// expclit construct from raw
ReferenceSkeleton::ReferenceSkeleton(SkeletonRuntimeAsset &&in_skeleton) {
    _skeleton = std::move(in_skeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(SkeletonRuntimeAsset &&in_skeleton) {
    _skeleton = std::move(in_skeleton);
    return *this;
}

luisa::span<const char *const> ReferenceSkeleton::raw_joint_names() const {
    auto raw_joint_names = _skeleton.joint_names();
    return {raw_joint_names.data(), raw_joint_names.size()};
}
luisa::span<const BoneIndexType> ReferenceSkeleton::raw_joint_parents() const {
    auto raw_joint_parents = _skeleton.joint_parents();
    return {raw_joint_parents.data(), raw_joint_parents.size()};
}
luisa::span<const AnimSOATransform> ReferenceSkeleton::joint_rest_poses() const {
    auto joint_rest_pose = _skeleton.joint_rest_poses();
    return {joint_rest_pose.data(), joint_rest_pose.size()};
}

BoneIndexType ReferenceSkeleton::get_parent_index(BoneIndexType in_bone_index) const {
    return raw_joint_parents()[in_bone_index];
}

void ReferenceSkeleton::log_brief() const {
    LUISA_INFO("skel has {} joints {} soa joints", num_joints(), num_soa_joints());
    const int kNumLogJoints = num_joints() > 10 ? 10 : num_joints();
    for (int i = 0; i < kNumLogJoints; i++) {
        auto &joint_name = raw_joint_names()[i];
        auto &parent = raw_joint_parents()[i];
        LUISA_INFO("skel {} <{}> has parent {}", i, joint_name, parent);
        int soa_idx = i / 4;
        const ozz::math::SoaTransform soa_pose = joint_rest_poses()[soa_idx];
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

void ReferenceSkeleton::ensure_parents_exist(luisa::vector<BoneIndexType> &in_out_bone_sorted_indices) const {
    // 保证bone indices是排序过的，这样从前向后轮询不会出错
    // TODO: 考虑采用ThreadSingleton
    const int32_t num_bones = get_num_bones();
    int32_t i = 0;
    luisa::vector<bool> bone_exists;
    bone_exists.resize(num_bones);
    while (i < in_out_bone_sorted_indices.size()) {
        const BoneIndexType bone_index = in_out_bone_sorted_indices[i];
        // For RootBone, Just move on
        if (bone_index == 0) {
            bone_exists[0] = true;
            i++;
            continue;
        }

        // bad data, warn and continue
        if (bone_index >= num_bones) {
            LUISA_ERROR("Bad Data for Skeleton Bone Index");
            i++;
            continue;
        }
        bone_exists[bone_index] = true;// make itself true
        const BoneIndexType parent_index = get_parent_index(bone_index);

        if (!bone_exists[parent_index]) {
            // 由于BoneIndices经过排序，所以parent必然在children之前出现，如果没有存在，则说明出现异常
            in_out_bone_sorted_indices.insert(in_out_bone_sorted_indices.begin() + i, parent_index);
            bone_exists[parent_index] = true;
        } else {
            i++;
        }
    }
}

void ReferenceSkeleton::ensure_parents_exist_and_sort(luisa::vector<BoneIndexType> &in_out_bone_unsorted_indices) const {
    std::sort(in_out_bone_unsorted_indices.begin(), in_out_bone_unsorted_indices.end());
    ensure_parents_exist(in_out_bone_unsorted_indices);
    std::sort(in_out_bone_unsorted_indices.begin(), in_out_bone_unsorted_indices.end());
}

}// namespace rbc

bool rbc::Serialize<rbc::ReferenceSkeleton>::write(rbc::ArchiveWrite &w, const rbc::ReferenceSkeleton &v) {
    // Use OzzStream in write mode - buffers all data internally
    OzzStream ozz_stream;
    ozz::io::OArchive archive(&ozz_stream);
    archive << v._skeleton;

    // Write the buffered data as a single bytes field
    auto buffer = ozz_stream.buffer();
    w.bytes(buffer, "data");

    return true;
}

bool rbc::Serialize<rbc::ReferenceSkeleton>::read(rbc::ArchiveRead &r, rbc::ReferenceSkeleton &v) {
    // Read the entire bytes blob first
    luisa::vector<std::byte> data;
    if (!r.bytes(data, "data")) {
        LUISA_ERROR("Failed to read skeleton data");
    }

    // Use OzzStream in read mode - provides sequential read from buffer
    OzzStream ozz_stream(luisa::span<const std::byte>{data.data(), data.size()});
    ozz::io::IArchive archive(&ozz_stream);
    archive >> v._skeleton;

    return true;
}