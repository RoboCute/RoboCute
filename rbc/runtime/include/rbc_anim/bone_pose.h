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
    luisa::vector<AnimSOATransform> bones;// 运行时进行Sampling的结果存放在这里

public:
    RBC_FORCEINLINE void InitBones(int NumBones) {
        uint32_t num_soa_bones = (NumBones + 3) / 4;
        LUISA_INFO("Init BasePose with {} SOABones", num_soa_bones);
        bones.resize_uninitialized(num_soa_bones);
    }
    RBC_FORCEINLINE int GetNumBones() const { return bones.size(); }

    RBC_FORCEINLINE AnimSOATransform &operator[](const BoneIndexType &BoneIndex) {
        return bones[BoneIndex.GetInt()];
    }
    RBC_FORCEINLINE const AnimSOATransform &operator[](const BoneIndexType &BoneIndex) const {
        return bones[BoneIndex.GetInt()];
    }
    luisa::vector<AnimSOATransform> &GetBones() {
        return bones;
    }
    const luisa::vector<AnimSOATransform> &GetBones() const { return bones; }
};

struct BaseCompactPose : BasePose<CompactPoseBoneIndex> {
public:
    void Clear();
    bool IsNormalized() const;
    bool IsValid() const;
    BoneContainer &GetBoneContainer();
    const BoneContainer &GetBoneContainer() const;
    void SetBoneContainer(const BoneContainer *InBoneContainer);
    void ResetToRefPose();
    void ResetToRefPose(const BoneContainer &InRequireBones);

    void CopyBonesFrom(const BaseCompactPose &SrcPose) {
        if (this != &SrcPose) {
            this->bones = SrcPose.GetBones();
            bone_container = &SrcPose.GetBoneContainer();
        }
    }

protected:
    const BoneContainer *bone_container;
};

struct CompactPose : BaseCompactPose {
public:
    void ResetToAdditiveIdentity();
    void NormalizeRotation();
};

}// namespace rbc