#pragma once
#include "rbc_anim/types.h"
#include "rbc_anim/bone_indices.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include <rbc_core/rc.h>

// Native Transient Container for Bones

namespace rbc {
struct SkeletalMesh;
}// namespace rbc

namespace rbc {

struct BoneContainer {

private:
    luisa::vector<BoneIndexType> _bone_indices;
    RCWeak<SkeletalMesh> _asset_skeletal_mesh;
    // Lookup Tables
    luisa::vector<int32_t> _compact_pose_to_skeleton_index;
    luisa::vector<int32_t> _skeleton_to_compact_pose;
    ReferenceSkeleton *_ref_skeleton;// the real referenced animation

public:
    BoneContainer();

public:
    void reset();
    void initialize_to(luisa::span<const BoneIndexType> required_bone_indices, SkeletalMesh *skel_mesh /*Config Placeholder*/);

    bool is_valid() const;
    luisa::vector<BoneIndexType> &get_bone_indices();
    const luisa::vector<AnimSOATransform> &get_ref_poses();
    const luisa::vector<BoneIndexType> &get_bone_indices() const;
    const ReferenceSkeleton *get_reference_skeleton() const { return _ref_skeleton; }

    int32_t get_num_bones() const {
        return _ref_skeleton->num_joints();
    }

    int32_t get_compact_pose_num_bones() const {
        return _bone_indices.size();
    }

public:
    template<typename ArrayType>
    void fill_with_compact_ref_pose(ArrayType &out_transform) const {
        const int32_t compact_pose_count = get_compact_pose_num_bones();
        // OutTransform.Reset()
        // OutTransform.SetNumUnInitialized();
        // override
        const auto &ref_pose = _ref_skeleton->joint_rest_poses();
        for (int32_t c_idx = 0; c_idx < compact_pose_count; ++c_idx) {
            out_transform[c_idx] = ref_pose[_bone_indices[c_idx]];
        }
    }

private:
    // Core Methods
    void _initialize();
    void _remap_from_skel_mesh();
    void _remap_from_skeleton();
};

}// namespace rbc
