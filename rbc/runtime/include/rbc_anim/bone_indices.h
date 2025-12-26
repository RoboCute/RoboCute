#pragma once
#include "rbc_anim/types.h"

namespace rbc {

constexpr BoneIndexType BONE_INDEX_NONE = -1;

struct BoneIndexBase {
    BoneIndexBase()
        : mBoneIndex(BONE_INDEX_NONE) {
    }

protected:
    int32_t mBoneIndex;
};

struct CompactPoseBoneIndex : public BoneIndexBase {
public:
    explicit CompactPoseBoneIndex(int32_t InBoneIndex) { mBoneIndex = InBoneIndex; }
};

struct MeshPoseBoneIndex : public BoneIndexBase {
public:
    explicit MeshPoseBoneIndex(int32_t InBoneIndex) { mBoneIndex = InBoneIndex; }
};

struct SkeletonPoseBoneIndex : public BoneIndexBase {
public:
    explicit SkeletonPoseBoneIndex(int32_t InBoneIndex) { mBoneIndex = InBoneIndex; }
};

}// namespace rbc