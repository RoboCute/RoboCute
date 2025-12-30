#include "rbc_anim/render/skelmesh_render.h"
#include "rbc_anim/skeletal_mesh.h"
#include <tracy_wrapper.h>

namespace rbc {

SkeletalMeshSceneProxyDesc::SkeletalMeshSceneProxyDesc(const SkeletalMesh *InSkelMesh) {
    skin_resource = InSkelMesh->ref_skin.get();
    mesh_resource = skin_resource->ref_mesh.get();
    skel_resource = skin_resource->ref_skel.get();
    render_data = InSkelMesh->render_data.get();
}

SkeletalMeshSceneProxyDynamicData::SkeletalMeshSceneProxyDynamicData(const SkeletalMesh *InSkelMesh) {
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

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMesh *InSkelMesh, RenderDevice *device)
    : SkeletalMeshRenderObject(SkeletalMeshSceneProxyDesc(InSkelMesh), device) {
}

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc, RenderDevice *device) : last_frame_number_(0), device_(device) {
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
    auto NumSkinJoints = InRefSkin->InverseBindPoses().size();

    ReferenceToLocal.resize_uninitialized(NumSkinJoints);

    for (auto i = 0; i < NumSkinJoints; i++) {
        // ReferenceToLocal[i] = AnimFloat4x4::identity();
        // ReferenceToLocal[i] = InRefSkin->inverse_bind_poses[i];
        ReferenceToLocal[i] = component_space_transforms[InRefSkin->JointRemapsLUT()[i]] * InRefSkin->InverseBindPoses()[i];
    }
}

}// namespace rbc
