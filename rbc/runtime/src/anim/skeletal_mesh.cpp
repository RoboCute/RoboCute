#include "rbc_anim/skeletal_mesh.h"
#include "rbc_anim/anim_instance.h"

namespace rbc {
bool SkeletalMesh::InitAnim() {
    if (bInitialized) { return true; }

    // if (!ref_skelmesh.is_installed()) {
    //     SKR_LOG_ERROR(u8"SkelMeshAsset not valid");
    //     return false;
    // }
    // auto *SkelMesh = GetSkelMeshResource().get_installed();
    // anim_instance = skr::ObjPtr<AnimInstance>::New();
    // anim_instance->InitAnimInstance(SkelMesh->RefAnimGraph);
    // anim_instance->BindSkelMesh(this);
    // anim_instance->InitializeAnimation();
    // const SkeletonResource *skel = GetRefSkeletonResource().get_installed();

    // if (!skel) {
    //     SKR_LOG_ERROR(u8"Skeleton Resource Should be valid when InitAnim");
    //     return false;
    // }
    // const SkinResource *skin = GetSkinResource().get_installed();
    // const MeshResource *mesh = GetSkinResource().get_installed()->ref_mesh.get_installed();
    // all done, init anim internally
    // InitAnim_Internal(skel, mesh, skin);
    bInitialized = true;
    return true;
}

void SkeletalMesh::AllocateTransformData() {
    // Get RefPose
    // if skinned mesh valid
    const int32_t NumBones = GetRefSkeleton().GetNumBones();
    if (GetNumComponentSpaceTransforms() != NumBones) {
        for (int32_t base_index = 0; base_index < 2; ++base_index) {
            LUISA_INFO("Allocating ComponentSpace Transform with %d Bones", NumBones);
            ComponentSpaceTransformsArray[base_index].clear();
            ComponentSpaceTransformsArray[base_index].resize_uninitialized(NumBones);
            // Initialize with Identity
            for (auto i = 0; i < NumBones; i++) {
                ComponentSpaceTransformsArray[base_index][i] = AnimFloat4x4::identity();
            }
        }
    }
    bHasValidBoneTransform = false;
    // Reset the animation stuff when changing mesh.
}

void SkeletalMesh::DeallocateTransformData() {
    LUISA_INFO("Deallocate Transform Data");
    for (int32_t base_index = 0; base_index < 2; ++base_index) {
        ComponentSpaceTransformsArray[base_index].clear();
    }
}

void SkeletalMesh::InitAnim_Internal() {
    AllocateTransformData();// 从asset中复制一份到当前SkeletalMesh作为运行时ComponentSpace的DualBuffer
    // Common Init
    RecalcRequiredBones(GetPredictedLODLevel());
    ResetToRefPose();
    RefreshBoneTransforms();
    // UpdateComponentToWorlds
}

// RAII Phase End
// ====================================================================

// ========================= GameThread Phase =========================
void SkeletalMesh::Tick(float InDeltaTime_s) {
    // LOD Changed?
    TickPose(InDeltaTime_s);// Do Update Here
    RefreshBoneTransforms();// Dispatch Evaluation Tasks Here
}

void SkeletalMesh::TickPose(float InDeltaTime_s) {
    // Control Tick Rate
    TickAnimation(InDeltaTime_s, true);
}

void SkeletalMesh::TickAnimation(float InDeltaTime_s, bool bNeedsValidRootMotion) {
    if (!bAnimationEnabled) { return; }
    if (!bRequiredBonesUpToDate) {
        RecalcRequiredBones(GetPredictedLODLevel());
    }
    // TickAnimationInstance
    // bNeedsQueuedAnimEventsDispatched = true
    TickAnimInstances(InDeltaTime_s, bNeedsValidRootMotion);
    // Conditionally Dispatch Events
}

void SkeletalMesh::TickAnimInstances(float InDeltaTime_s, bool bNeedsValidRootMotion) {
    if (!bAnimationEnabled) { return; }
    // {PreUpdateLinkedInstances}
    // {LinkedInstance->UpdateAnimation}
    if (anim_instance) {
        anim_instance->UpdateAnimation(InDeltaTime_s, bNeedsValidRootMotion, AnimInstance::EUpdateAnimationFlag::ForceParallelUpdate);
    }
}
// GameThread Tick Phase End
// ====================================================================

void SkeletalMesh::FlipEditableSpaceBases() {
    if (bNeedToFlipSpaceBaseBuffers) {
        bNeedToFlipSpaceBaseBuffers = false;
        CurrentEditableComponentTransformsIdx = 1 - CurrentEditableComponentTransformsIdx;
        CurrentReadComponentTransformsIdx = 1 - CurrentReadComponentTransformsIdx;
        // Update Queue
        // Update Revision Number
    }
}

void SkeletalMesh::SetBoneSpaceTransforms(luisa::span<const AnimSOATransform> InBoneSpaceTransforms) {
    BoneSpaceTransforms.resize_uninitialized(InBoneSpaceTransforms.size());
    for (auto i = 0; i < InBoneSpaceTransforms.size(); i++) {
        BoneSpaceTransforms[i] = InBoneSpaceTransforms[i];
    }
}

bool SkeletalMesh::ShouldBlendPhysicsBones() {
    return false;// 当前不需要考虑PhysicsBones
}

luisa::shared_ptr<BoneContainer> SkeletalMesh::GetSharedRequiredBones() {
    if (shared_required_bones == nullptr) {
        shared_required_bones = luisa::make_shared<BoneContainer>();
    }
    return shared_required_bones;
}

void SkeletalMesh::ResetToRefPose() {
    // asset rest pose -> BoneSpace
    auto rest_view = GetRefSkeleton().JointRestPoses();
    SetBoneSpaceTransforms(rest_view);
    // Initial BoneSpace -> Editing Component Space
    FillComponentSpaceTransforms(BoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, GetEditableComponentSpaceTransforms());

    // Editing ComponentSpace -> Reading ComponentSpace
    //! Flip Editable Space Bases 1
    bNeedToFlipSpaceBaseBuffers = true;
    FlipEditableSpaceBases();
}

void SkeletalMesh::RefreshBoneTransforms() {
    if (!bAnimationEnabled) { return; }
    // recalc required bones when update
    // recalc required corves
    // if (!ref_skelmesh.is_installed()) {
    //     SKR_LOG_ERROR(u8"SkelMeshAsset not valid");
    //     return;
    // }
    const bool bShouldDoEvaluation = true;
    const bool bShouldDoInterpolation = false;
    const bool bDoParallelEvaluation = true;

    //! Initialize Anim Evaluation Context ==> CompleteParallelAnimationEvaluation to clear
    anim_eval_context.Init(anim_instance.get(), this);

    anim_eval_context.bDoEvaluation = bShouldDoEvaluation;
    anim_eval_context.bDoInterpolation = bShouldDoInterpolation;

    // Duplicate to cache bones
    // force ref pos
    // dispatch task
    if (bDoParallelEvaluation) {
        DispatchParallelEvaluationTasks();
    }
}

void SkeletalMesh::RecalcRequiredBones(int32_t LODIndex) {
    ComputeRequiredBones(RequiredBones, FillComponentSpaceTransformsRequiredBones, LODIndex);
    // Reset Anim Pose to Reference Pose
    auto ref_pose = GetRefSkeleton().JointRestPoses();
    BoneSpaceTransforms.resize_default(ref_pose.size());
    for (auto i = 0; i < ref_pose.size(); i++) {
        BoneSpaceTransforms[i] = ref_pose[i];
    }

    // Clear Cached Bone Containers
    if (anim_instance) {
        anim_instance->RecalcRequiredBones();
    }
    // For Linked Instance, RecalcRequiredBones
    // For PostAnimInstance, RecalcRequiredBones
    // Mark Curve Up to date
    // Invalidate Cached Bones
    bRequiredBonesUpToDate = true;
    // Broadcast Messages
}
void SkeletalMesh::RecalcRequiredCurves() {
    // NOT IMPLEMENTED
}

void SkeletalMesh::ComputeRequiredBones(luisa::vector<BoneIndexType> &OutRequiredBones, luisa::vector<BoneIndexType> &OutFillComponentSpaceTransformRequiredBones, int32_t LODIndex) const {
    OutRequiredBones.clear();
    OutFillComponentSpaceTransformRequiredBones.clear();

    // auto *skelmesh = GetSkelMeshResource().get_installed();
    // SkeletalMeshRenderData *render_data = skelmesh->render_data.get();
    // if (!render_data) {
    //     SKR_LOG_ERROR(u8"SkeletalMesh has no render data!");
    //     return;
    // }
    // OutRequiredBones = render_data->required_bones;// Copy from render data
    // AnimationRuntime::EnsureParentsPresent(OutRequiredBones, GetRefSkeleton());
    // OutRequiredBones.sort();
}

// void SkeletalMesh::CreateRenderState_Concurrent(skr::RenderDevice *InRenderDevice) {
//     if (bUseGPUSkin) {
//         render_object_ = SkrNew<SkeletalMeshRenderObjectGPUSkin>(this, InRenderDevice);
//     } else {
//         render_object_ = SkrNew<SkeletalMeshRenderObjectCPUSkin>(this, InRenderDevice);
//     }
// }
// void SkeletalMesh::DestroyRenderState_Concurrent() {
//     if (render_object_) {
//         render_object_->ReleaseResources();
//         SkrDelete(render_object_);
//     }

//     DeallocateTransformData();
// }
// void SkeletalMesh::DoDeferredRenderUpdate_Concurrent(AnimRenderState &state) {
//     if (bRenderTransformDirty) {
//     }
//     if (bRenderDynamicDataDirty) {
//         SendRenderDynamicData_Concurrent(state);
//     }
// }

// void SkeletalMesh::SendRenderDynamicData_Concurrent(AnimRenderState &state) {
//     bRenderDynamicDataDirty = false;
//     {
//         // cycle counter
//         int32_t useLOD = GetPredictedLODLevel();

//         SkinResource *ref_skin = GetSkinResource().get_installed();
//         if (ref_skin) {
//             SkeletalMeshSceneProxyDynamicData data{this};
//             render_object_->Update(state, 0, data, ref_skin);
//             bForceMeshObjectUpdate = false;
//         }
//     }
// }

void SkeletalMesh::PerformAnimationProcessing(SkeletalMesh *InSkeletalMesh, AnimInstance *InAnimInstance, bool bInDoEvaluation, bool bForceRefPose, luisa::vector<AnimSOATransform> &OutBoneSpaceTransforms, luisa::vector<AnimFloat4x4> &OutComponentSpaceTransforms) {
    if (!InSkeletalMesh) { return; }
    // update anim instance
    if (InAnimInstance && InAnimInstance->NeedsUpdate()) {
        InAnimInstance->ParallelUpdateAnimation();
    }

    // do nothing of no output required
    // SKR_LOG_FMT_INFO(u8"Processing Animation Process withOutBoneSpaceTransforms {}", OutBoneSpaceTransforms.size());

    if (bInDoEvaluation && OutBoneSpaceTransforms.size() > 0) {
        CompactPose EvaluatedPose;
        EvaluateAnimation(InSkeletalMesh, InAnimInstance, bForceRefPose, EvaluatedPose);
        EvaluatePostProcessMeshInstance();
        FinalizePoseEvaluationResult(this, OutBoneSpaceTransforms, EvaluatedPose);
        // FinalizeAttributeEvaluationResults(EvaluatedPose.GetBoneContainer(), Attributes, OutAttributes);
        // LocalAtoms
        InSkeletalMesh->FillComponentSpaceTransforms(OutBoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, OutComponentSpaceTransforms);
    }
}

void SkeletalMesh::EvaluateAnimation(SkeletalMesh *InSkelMesh, AnimInstance *InAnimInstance, bool bInForceRefPose, CompactPose &OutPose) const {
    // OutputRootBoneTranslation
    // OutCurve
    // OutPose
    // OutAttributes
    if (!InSkelMesh) {
        return;
    }

    {
        // Construct Evaluation Data
        ParallelEvaluationData OutData{OutPose};
        InAnimInstance->ParallelEvaluateAnimation(bInForceRefPose, InSkelMesh, OutData);
    }
}

void SkeletalMesh::EvaluatePostProcessMeshInstance() {
    // placeholder for postprocess eval
}

void SkeletalMesh::ParallelUpdateAnimation() {
    // actual update animation instance state
    // SKR_LOG_INFO(u8"ParallelUpdateAnimation");
}

void SkeletalMesh::ParallelDuplicateAndInterpolate() {}

luisa::vector<AnimFloat4x4> &SkeletalMesh::GetEditableComponentSpaceTransforms() {
    return ComponentSpaceTransformsArray[CurrentEditableComponentTransformsIdx];
}

const luisa::vector<AnimFloat4x4> &SkeletalMesh::GetComponentSpaceTransforms() const {
    return ComponentSpaceTransformsArray[CurrentReadComponentTransformsIdx];
}

size_t SkeletalMesh::GetNumComponentSpaceTransforms() const {
    return GetComponentSpaceTransforms().size();
}

//! IMPORTANT: Perform Local To Model Job
void SkeletalMesh::FillComponentSpaceTransforms(luisa::span<const AnimSOATransform> InBoneSpaceTransforms, luisa::span<const BoneIndexType> InFillComponentSpaceTransformsRequiredBones, luisa::vector<AnimFloat4x4> &OutComponentSpaceTransforms) const {
    const auto NumBones = InBoneSpaceTransforms.size();
    if (NumBones <= 0) {
        return;
    }

    AnimLocalToModelJob ltm_job;
    ltm_job.skeleton = &(GetRefSkeleton().GetRawSkeleton());
    ltm_job.input = {
        (const AnimSOATransform *)InBoneSpaceTransforms.data(),
        InBoneSpaceTransforms.size()};
    ltm_job.output = {
        (AnimFloat4x4 *)OutComponentSpaceTransforms.data(),
        OutComponentSpaceTransforms.size()};
    if (!ltm_job.Run()) {
        LUISA_ERROR("Failed to run LocalToModelJob");
    }
}// namespace skr

void SkeletalMesh::SwapEvaluationContextBuffers() {
    std::swap(anim_eval_context.BoneSpaceTransforms, BoneSpaceTransforms);
    std::swap(anim_eval_context.ComponentSpaceTransforms, GetEditableComponentSpaceTransforms());
}

/**
 * FinalizePoseEvaluationResult
 * ====================================================
 * Get output bone space transforms from final pose generated by AnimGraph execution
 */
void SkeletalMesh::FinalizePoseEvaluationResult(const SkeletalMesh *InSkelMesh, luisa::vector<AnimSOATransform> &OutBoneSpaceTransforms, CompactPose &InFinalPose) {
    const luisa::span<const AnimSOATransform> ref_bone_pose = GetRefSkeleton().JointRestPoses();
    OutBoneSpaceTransforms.resize_uninitialized(ref_bone_pose.size());
    for (auto i = 0; i < ref_bone_pose.size(); i++) {
        OutBoneSpaceTransforms[i] = ref_bone_pose[i];
    }

    if (InFinalPose.IsValid() && InFinalPose.GetNumBones() > 0) {

        InFinalPose.NormalizeRotation();
        const auto FillReferencePose = [&](int32_t begin_index, int32_t end_index) {
            for (int32_t mesh_pose_index = begin_index; mesh_pose_index < end_index; ++mesh_pose_index) {
                OutBoneSpaceTransforms[mesh_pose_index] = ref_bone_pose[mesh_pose_index];
            }
        };

        int32_t last_pose_index = 0;
        const int32_t bone_count = ref_bone_pose.size();
        // OutBoneSpaceTransforms.resize_default(bone_count);
        for (auto i = 0; i < bone_count; i++) {
            OutBoneSpaceTransforms[i] = InFinalPose.GetBones()[i];
        }
    }
}

void SkeletalMesh::FinalizeBoneTransforms() {
    FlipEditableSpaceBases();// 将写入完成的BoneSpace翻转到读取Space
    bHasValidBoneTransform = true;
}

void SkeletalMesh::FinalizeAnimationUpdate() {
    // flip bone buffers and post anim notification
    FinalizeBoneTransforms();
    // [NOT_IMPLEMENTED]: UpdateChildTransforms() UpdateOverlaps() UpdateBounds()
    MarkRenderTransformDirty();
    MarkRenderDynamicDataDirty();
}

void SkeletalMesh::PostAnimEvaluation() {
    // actual do post anim evaluation
    // - If Interpolation

    if (bDuplicateToCacheBones) {
    }
    if (bDoEvaluation) {
        // update curves
        // update morph targets
        // DoInstanceEvaluation()
        //! 更新状态，需要翻转骨骼
        bNeedToFlipSpaceBaseBuffers = true;

        if (!ShouldBlendPhysicsBones()) {
            // flip bone buffer and request render
            FinalizeAnimationUpdate();
        }
    }
}

// get & set
void SkeletalMesh::MarkRenderTransformDirty() { bRenderTransformDirty = true; }
void SkeletalMesh::MarkRenderDynamicDataDirty() { bRenderDynamicDataDirty = true; }
bool SkeletalMesh::IsDynamicDataDirty() const { return bRenderDynamicDataDirty; }
void SkeletalMesh::EnableAnimation() { bAnimationEnabled = true; }
void SkeletalMesh::DisableAnimation() { bAnimationEnabled = false; }
bool SkeletalMesh::IsAnimationEnabled() const { return bAnimationEnabled; }

ReferenceSkeleton &SkeletalMesh::GetRefSkeleton() {
    // SKR_ASSERT(ref_skeleton.is_installed());
    // return ref_skeleton.get_installed()->skeleton;
}

RC<SkeletonResource> &SkeletalMesh::GetRefSkeletonResource() {
    return ref_skeleton;
}
const ReferenceSkeleton &SkeletalMesh::GetRefSkeleton() const {
    // SKR_ASSERT(ref_skeleton.is_installed());
    // return ref_skeleton.get_installed()->skeleton;
    return *ref_skeleton.get();
}

const<SkeletonResource> &SkeletalMesh::GetRefSkeletonResource() const {
    return ref_skeleton;
}

// Task Interface
// ================================================================================
// 这部分函数会在未来改为Workgraph异步Task，在开发过程中暂时保持同步，方便其他部分
// ================================================================================
void SkeletalMesh::DispatchParallelEvaluationTasks() {
    SwapEvaluationContextBuffers();// edit buffers -> context buffers
    ParallelAnimationEvaluation(); // do evaluation

    CompleteParallelAnimationEvaluation(true);
}

void SkeletalMesh::ParallelAnimationEvaluation() {
    if (anim_eval_context.bDoInterpolation) {
        PerformAnimationProcessing(
            anim_eval_context.skel_mesh,
            anim_eval_context.anim_instance,
            anim_eval_context.bDoEvaluation,
            false,
            anim_eval_context.CachedBoneSpaceTransforms,
            anim_eval_context.CachedComponentSpaceTransforms);
    } else {
        PerformAnimationProcessing(
            anim_eval_context.skel_mesh,
            anim_eval_context.anim_instance,
            anim_eval_context.bDoEvaluation,
            false,
            anim_eval_context.BoneSpaceTransforms,
            anim_eval_context.ComponentSpaceTransforms);
    }

    // actual sampling and fetch current pose
    // SKR_LOG_INFO(u8"ParallelEvaluateAnimationAnimation");
}

void SkeletalMesh::CompleteParallelAnimationEvaluation(bool bDoPostAnimEvaluation) {
    // TODO: Release current ParallelEvaluationTask list
    if (bDoPostAnimEvaluation) {
        // swap back
        SwapEvaluationContextBuffers();
        PostAnimEvaluation();
    }
    anim_eval_context.Clear();
}

// ================================================================================
// Task Interface End
// ================================================================================

}// namespace rbc