#pragma once
// Bone/Pose/Curves
#include "rbc_config.h"
#include "rbc_anim/types.h"
#include "rbc_anim/bone_indices.h"

namespace rbc {
struct BoneContainer;
}// namespace rbc

namespace rbc {

template<class BoneIndexType>
struct BasePose {
protected:
    luisa::vector<AnimSOATransform> _bones;// 运行时进行Sampling的结果存放在这里

public:
    RBC_FORCEINLINE void init_bones(int num_bones) {
        uint32_t num_soa_bones = (num_bones + 3) / 4;
        // LUISA_INFO("Init BasePose with {} SOABones", num_soa_bones);
        _bones.resize_uninitialized(num_soa_bones);
    }
    RBC_FORCEINLINE int get_num_bones() const { return _bones.size(); }

    RBC_FORCEINLINE AnimSOATransform &operator[](const BoneIndexType &bone_index) {
        return _bones[bone_index.GetInt()];
    }
    RBC_FORCEINLINE const AnimSOATransform &operator[](const BoneIndexType &bone_index) const {
        return _bones[bone_index.GetInt()];
    }
    luisa::vector<AnimSOATransform> &get_bones() {
        return _bones;
    }
    const luisa::vector<AnimSOATransform> &get_bones() const { return _bones; }
};

struct BaseCompactPose : BasePose<CompactPoseBoneIndex> {
public:
    void clear();
    bool is_normalized() const;
    bool is_valid() const;
    BoneContainer &get_bone_container();
    const BoneContainer &get_bone_container() const;
    void set_bone_container(const BoneContainer *bone_container);
    void reset_to_ref_pose();
    void reset_to_ref_pose(const BoneContainer &required_bones);

    void copy_bones_from(const BaseCompactPose &src_pose) {
        if (this != &src_pose) {
            this->_bones = src_pose.get_bones();
            _bone_container = &src_pose.get_bone_container();
        }
    }

protected:
    const BoneContainer *_bone_container;
};

struct CompactPose : BaseCompactPose {
public:
    void reset_to_additive_identity();
    void normalize_rotation();
};

}// namespace rbc