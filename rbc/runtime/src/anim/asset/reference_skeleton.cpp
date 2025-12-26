#include <rbc_anim/asset/reference_skeleton.h>
#include <rbc_anim/asset/ozz_stream.h>
#include <ozz/base/io/archive.h>

namespace rbc {
ReferenceSkeleton::ReferenceSkeleton() = default;
ReferenceSkeleton::~ReferenceSkeleton() {}
ReferenceSkeleton::ReferenceSkeleton(ReferenceSkeleton &&Other) noexcept {
    skeleton = std::move(Other.skeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(ReferenceSkeleton &&Other) noexcept {
    skeleton = std::move(Other.skeleton);
    return *this;
}
// expclit construct from raw
ReferenceSkeleton::ReferenceSkeleton(SkeletonRuntimeAsset &&InSkeleton) {
    skeleton = std::move(InSkeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(SkeletonRuntimeAsset &&InSkeleton) {
    skeleton = std::move(InSkeleton);
    return *this;
}

luisa::span<const char *const> ReferenceSkeleton::RawJointNames() const {
    auto raw_joint_names = skeleton.joint_names();
    return {raw_joint_names.data(), raw_joint_names.size()};
}
luisa::span<const BoneIndexType> ReferenceSkeleton::RawJointParents() const {
    auto raw_joint_parents = skeleton.joint_parents();
    return {raw_joint_parents.data(), raw_joint_parents.size()};
}
luisa::span<const AnimSOATransform> ReferenceSkeleton::JointRestPoses() const {
    auto joint_rest_pose = skeleton.joint_rest_poses();
    return {joint_rest_pose.data(), joint_rest_pose.size()};
}

BoneIndexType ReferenceSkeleton::GetParentIndex(BoneIndexType InBoneIndex) const {
    return RawJointParents()[InBoneIndex];
}

void ReferenceSkeleton::EnsureParentsExist(luisa::vector<BoneIndexType> &InOutBoneSortedIndices) const {
    // 保证bone indices是排序过的，这样从前向后轮询不会出错
    // TODO: 考虑采用ThreadSingleton
    const int32_t num_bones = GetNumBones();
    int32_t i = 0;
    luisa::vector<bool> bone_exists;
    bone_exists.resize(num_bones);
    while (i < InOutBoneSortedIndices.size()) {
        const BoneIndexType bone_index = InOutBoneSortedIndices[i];
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
        const BoneIndexType parent_index = GetParentIndex(bone_index);    

        if (!bone_exists[parent_index]) {
            // 由于BoneIndices经过排序，所以parent必然在children之前出现，如果没有存在，则说明出现异常
            InOutBoneSortedIndices.insert(InOutBoneSortedIndices.begin() + i, parent_index);
            bone_exists[parent_index] = true;
        } else {
            i++;
        }
    }
}

void ReferenceSkeleton::EnsureParentsExistAndSort(luisa::vector<BoneIndexType> &InOutBoneUnsortedIndices) const {
    std::sort(InOutBoneUnsortedIndices.begin(), InOutBoneUnsortedIndices.end());
    EnsureParentsExist(InOutBoneUnsortedIndices);
    std::sort(InOutBoneUnsortedIndices.begin(), InOutBoneUnsortedIndices.end());
}

}// namespace rbc

void rbc::Serialize<rbc::ReferenceSkeleton>::write(rbc::ArchiveWrite &w, const rbc::ReferenceSkeleton &v) {
    // Use OzzStream in write mode - buffers all data internally
    OzzStream ozz_stream;
    ozz::io::OArchive archive(&ozz_stream);
    archive << v.skeleton;
    
    // Write the buffered data as a single bytes field
    auto buffer = ozz_stream.buffer();
    w.bytes(buffer, "data");
}

bool rbc::Serialize<rbc::ReferenceSkeleton>::read(rbc::ArchiveRead &r, rbc::ReferenceSkeleton &v) {
    // Read the entire bytes blob first
    luisa::vector<std::byte> data;
    if (!r.bytes(data, "data")) {
        LUISA_ERROR("Failed to read skeleton data");
        return false;
    }
    
    // Use OzzStream in read mode - provides sequential read from buffer
    OzzStream ozz_stream(luisa::span<const std::byte>{data.data(), data.size()});
    ozz::io::IArchive archive(&ozz_stream);
    archive >> v.skeleton;
    return true;
}