#include "rbc_anim/render/skelmesh_render.h"
#include <tracy_wrapper.h>

namespace rbc {

SkeletalMeshSceneProxyDesc::SkeletalMeshSceneProxyDesc(const SkeletalMesh *InSkelMesh)
    : skin_resource(InSkelMesh->GetSkinResource().get_installed()), skelmesh_resource(InSkelMesh->GetSkelMeshResource().get_installed()) {
    mesh_resource = skin_resource->ref_mesh.get_installed();
    skel_resource = skin_resource->ref_skeleton.get_installed();
    render_data = skelmesh_resource->render_data.get();
}

SkeletalMeshSceneProxyDynamicData::SkeletalMeshSceneProxyDynamicData(const SkeletalMesh *InSkelMesh)
    : component_space_transforms_(InSkelMesh->GetComponentSpaceTransforms()) {
}

bool SkeletalMeshSceneProxyDynamicData::IsSkinCacheAllowed() const {
    return true;
}
uint32_t SkeletalMeshSceneProxyDynamicData::GetBoneTransformRevisionNumber() const {
    RBC_UNIMPLEMENTED();
    return 0;
}
uint32_t SkeletalMeshSceneProxyDynamicData::GetPreviousBoneTransformRevisionNumber() const {
    RBC_UNIMPLEMENTED();
    return 0;
}
uint32_t SkeletalMeshSceneProxyDynamicData::GetCurrentBoneTransformFrame() const {
    RBC_UNIMPLEMENTED();
    return 0;
}
int32_t SkeletalMeshSceneProxyDynamicData::GetNumLODs() const {
    RBC_UNIMPLEMENTED();
    return 0;
}

// Core Data
luisa::span<const AnimFloat4x4> SkeletalMeshSceneProxyDynamicData::GetComponentSpaceTransforms() const {
    return component_space_transforms_;
}

luisa::span<const AnimFloat4x4> SkeletalMeshSceneProxyDynamicData::GetPreviousComponentSpaceTransforms() const {
    return component_space_transforms_;
}

}// namespace rbc

namespace rbc {

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMesh *InSkelMesh)
    : SkeletalMeshRenderObject(SkeletalMeshSceneProxyDesc(InSkelMesh)) {
}

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc) : last_frame_number_(0) {
}

SkeletalMeshRenderObject::~SkeletalMeshRenderObject() {}

//============================================================================
//============================ Utility Functions =============================

/**
 * UpdateRefToLocalMatrices
 * ======================================
 * Utility Function That Compute
 * * ReferenceToLocal matrices
 required by the skinning procedural from
 * * Evaluated Bones (InDynamicData)
 * * Inverse Binding Matrices from Reference Skin Resource
 */
//! IMPORTANT: ComponentSpaceTransform -> RefToLocal
void UpdateRefToLocalMatrices(luisa::vector<AnimFloat4x4> &ReferenceToLocal, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin) {
    RBCZoneScopedN("UpdateRefToLocalMatrices");
    const auto component_space_transforms = InDynamicData.GetComponentSpaceTransforms();
    auto NumBones = component_space_transforms.size();
    auto NumSkinJoints = InRefSkin->inverse_bind_poses.size();
    ReferenceToLocal.resize_uninitialized(NumSkinJoints);

    for (auto i = 0; i < NumSkinJoints; i++) {
        // ReferenceToLocal[i] = AnimFloat4x4::identity();
        // ReferenceToLocal[i] = InRefSkin->inverse_bind_poses[i];
        ReferenceToLocal[i] = component_space_transforms[InRefSkin->joint_remaps_LUT[i]] * InRefSkin->inverse_bind_poses[i];
    }
}

}// namespace rbc
