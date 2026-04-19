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
    ReferenceSkeleton(ReferenceSkeleton &&other) noexcept;
    ReferenceSkeleton &operator=(ReferenceSkeleton &&other) noexcept;
    explicit ReferenceSkeleton(SkeletonRuntimeAsset &&in_skeleton);
    ReferenceSkeleton &operator=(SkeletonRuntimeAsset &&in_skeleton);

public:
    [[nodiscard]] const SkeletonRuntimeAsset &get_raw_skeleton() const { return _skeleton; }
    SkeletonRuntimeAsset &get_raw_skeleton() { return _skeleton; }
    [[nodiscard]] int32_t get_num_bones() const { return num_joints(); }
    [[nodiscard]] int num_joints() const { return _skeleton.num_joints(); }
    [[nodiscard]] int num_soa_joints() const { return _skeleton.num_soa_joints(); }
    [[nodiscard]] luisa::span<const char *const> raw_joint_names() const;
    [[nodiscard]] luisa::span<const BoneIndexType> raw_joint_parents() const;
    [[nodiscard]] luisa::span<const AnimSOATransform> joint_rest_poses() const;
    [[nodiscard]] BoneIndexType get_parent_index(BoneIndexType in_bone_index) const;

    void log_brief() const;// debug helper

public:
    void ensure_parents_exist(luisa::vector<BoneIndexType> &in_out_bone_sorted_indices) const;
    void ensure_parents_exist_and_sort(luisa::vector<BoneIndexType> &in_out_bone_unsorted_indices) const;

private:
    friend struct Serialize<ReferenceSkeleton>;
    SkeletonRuntimeAsset _skeleton;
};

}// namespace rbc

template<>
struct rbc::Serialize<rbc::ReferenceSkeleton> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::ReferenceSkeleton &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::ReferenceSkeleton &v);
};