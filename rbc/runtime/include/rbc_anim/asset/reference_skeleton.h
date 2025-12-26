#pragma once

#include <luisa/vstl/vector.h>
#include "rbc_core/serde.h"
#include "rbc_anim/types.h"

namespace rbc {

struct RBC_RUNTIME_API ReferenceSkeleton final {
public:
    ReferenceSkeleton();
    ~ReferenceSkeleton();
    ReferenceSkeleton(const ReferenceSkeleton &) = delete;
    ReferenceSkeleton &operator=(const ReferenceSkeleton &) = delete;
    ReferenceSkeleton(ReferenceSkeleton &&Other) noexcept;
    ReferenceSkeleton &operator=(ReferenceSkeleton &&Other) noexcept;
    explicit ReferenceSkeleton(SkeletonRuntimeAsset &&InSkeleton);
    ReferenceSkeleton &operator=(SkeletonRuntimeAsset &&InSkeleton);

public:
    [[nodiscard]] const SkeletonRuntimeAsset &GetRawSkeleton() const { return skeleton; }
    SkeletonRuntimeAsset &GetRawSkeleton() { return skeleton; }
    [[nodiscard]] int32_t GetNumBones() const { return NumJoints(); }
    [[nodiscard]] int NumJoints() const { return skeleton.num_joints(); }
    [[nodiscard]] int NumSOAJoints() const { return skeleton.num_soa_joints(); }
    [[nodiscard]] luisa::span<const char *const> RawJointNames() const;
    [[nodiscard]] luisa::span<const BoneIndexType> RawJointParents() const;
    [[nodiscard]] luisa::span<const AnimSOATransform> JointRestPoses() const;
    [[nodiscard]] BoneIndexType GetParentIndex(BoneIndexType InBoneIndex) const;

    void log_brief();// debug helper

public:
    void EnsureParentsExist(luisa::vector<BoneIndexType> &InOutBoneSortedIndices) const;
    void EnsureParentsExistAndSort(luisa::vector<BoneIndexType> &InOutBoneUnsortedIndices) const;

private:
    friend struct Serialize<ReferenceSkeleton>;
    SkeletonRuntimeAsset skeleton;
};

}// namespace rbc

template<>
struct rbc::Serialize<rbc::ReferenceSkeleton> {
    static RBC_RUNTIME_API void write(rbc::ArchiveWrite &w, const rbc::ReferenceSkeleton &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::ReferenceSkeleton &v);
};