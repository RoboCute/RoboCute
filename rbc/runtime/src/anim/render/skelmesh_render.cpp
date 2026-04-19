#include "rbc_anim/render/skelmesh_render.h"
#include "rbc_anim/skeletal_mesh.h"
#include <tracy_wrapper.h>
#include <algorithm>

namespace rbc {

SkeletalMeshSceneProxyDesc::SkeletalMeshSceneProxyDesc(const SkeletalMesh *in_skel_mesh) {
    skin_resource = in_skel_mesh->ref_skin.get();
    mesh_resource = skin_resource->ref_mesh.get();
    skel_resource = skin_resource->ref_skel.get();
    render_data = in_skel_mesh->render_data.get();
}

SkeletalMeshSceneProxyDynamicData::SkeletalMeshSceneProxyDynamicData(const SkeletalMesh *in_skel_mesh) : component_space_transforms_(in_skel_mesh->GetComponentSpaceTransforms()) {
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

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMesh *in_skel_mesh, RenderDevice *device)
    : SkeletalMeshRenderObject(SkeletalMeshSceneProxyDesc(in_skel_mesh), device) {
}

SkeletalMeshRenderObject::SkeletalMeshRenderObject(const SkeletalMeshSceneProxyDesc &in_skel_mesh_desc, RenderDevice *device) : last_frame_number_(0), device_(device) {
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
void UpdateRefToLocalMatrices(luisa::vector<AnimFloat4x4> &reference_to_local, const SkeletalMeshSceneProxyDynamicData &in_dynamic_data, const world::SkinResource *in_ref_skin) {
    RBCZoneScopedN("UpdateRefToLocalMatrices");
    const auto component_space_transforms = in_dynamic_data.GetComponentSpaceTransforms();
    const auto num_skin_joints = in_ref_skin->InverseBindPoses().size();

    reference_to_local.resize_uninitialized(num_skin_joints);

    const auto joint_remaps = in_ref_skin->JointRemapsLUT();
    const auto inverse_bind_poses = in_ref_skin->InverseBindPoses();

    std::transform(
        inverse_bind_poses.begin(), inverse_bind_poses.end(),
        joint_remaps.begin(),
        reference_to_local.begin(),
        [&component_space_transforms](const auto &inverse_bind_pose, auto remap_index) {
            return component_space_transforms[remap_index] * inverse_bind_pose;
        });
}

}// namespace rbc
