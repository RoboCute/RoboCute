
#include "rbc_core/memory.h"
#include "rbc_world/base_object.h"
#include "rbc_anim/bone_container.h"
#include "rbc_anim/anim_instance.h"
#include "rbc_anim/graph/AnimGraph.h"
#include "rbc_world/resources/anim_graph.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc {

void AnimInstance::InitAnimInstance(RC<AnimGraphResource> &InAnimGraph) {
    LUISA_INFO("Init AnimInstance");
    anim_graph = InAnimGraph;
}

void AnimInstance::BindSkelMesh(SkeletalMesh *InSkelMesh) {
    LUISA_INFO("[AnimInstance] Binding SkelMesh");
    skel_mesh = InSkelMesh;
}
SkeletalMesh *AnimInstance::GetSkeletalMesh() const {
    return skel_mesh;
}
AnimGraph *AnimInstance::GetAnimGraph() {
    return &(anim_graph->graph);
}

void AnimInstance::InitializeAnimation() {
    LUISA_INFO("[AnimInstance] Init AnimProxy");
    GetProxyOnGameThread<AnimInstanceProxy>().Initialize(this);
}

AnimInstanceProxy *AnimInstance::CreateAnimInstanceProxy() {
    return RBCNew<AnimInstanceProxy>(this);
}

void AnimInstance::DestroyAnimInstanceProxy(AnimInstanceProxy *InProxy) {
    RBCDelete(InProxy);
}

void AnimInstance::RecalcRequiredBones() {
    auto *skel_mesh = GetSkeletalMesh();
    GetProxyOnGameThread<AnimInstanceProxy>().RecalcRequiredBones(skel_mesh);

    // TODO: use proxy impl
}
void AnimInstance::RecalcRequiredCurves() {
    // TODO: use proxy impl
}

bool AnimInstance::NeedsUpdate() {
    return true;
}

void AnimInstance::ParallelUpdateAnimation() {
    // SKR_LOG_INFO(u8"Anim Instance ParallelUpdateAnimation");
    GetProxyOnAnyThread<AnimInstanceProxy>().UpdateAnimation();
}

// Update阶段的入口
void AnimInstance::UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion, EUpdateAnimationFlag UpdateFlag) {

    PreUpdateAnimation(DeltaSeconds);
    {
        // Update Montage
        // Update Montage Sync Group
        // Update Montage Evaluation Data
    }
    // Update Subsystem
    {
        // Native Update Animation
    }
    {
        // Blueprint Update Animation
    }
    bool bShouldImmediateUpdate = true;
    if (bShouldImmediateUpdate) {
        ParallelUpdateAnimation();
        PostUpdateAnimation();
    }
}

void AnimInstance::PreUpdateAnimation(float DeltaSeconds) {
    bNeedsUpdate = true;
    // Update Delta Time Should be running on GameThread
    GetProxyOnGameThread<AnimInstanceProxy>().PreUpdate(this, DeltaSeconds);
}

void AnimInstance::PostUpdateAnimation() {
    // Flip Read/Write Index
    // Post Update
    // Parallel Blended?
}

void AnimInstance::ParallelEvaluateAnimation(bool bForceRefPose, const SkeletalMesh *InSkeletalMesh, ParallelEvaluationData &OutData) {
    auto &AnimProxy = GetProxyOnAnyThread<AnimInstanceProxy>();

    OutData.OutPose.SetBoneContainer(&AnimProxy.GetRequiredBones());

    if (!bForceRefPose) {
        PoseContext eval_context(&AnimProxy);
        eval_context.ResetToRefPose();
        // Run AnimGraph
        AnimProxy.EvaluateAnimation(eval_context);
        OutData.OutPose.CopyBonesFrom(eval_context.Pose);
    } else {
        OutData.OutPose.ResetToRefPose();
    }
}

}// namespace rbc

namespace rbc {

AnimInstanceProxy::AnimInstanceProxy()
    : AnimInstanceObject(nullptr), current_delta_seconds(0.0f) {
}

AnimInstanceProxy::AnimInstanceProxy(AnimInstance *InAnimInstance)
    : AnimInstanceObject(InAnimInstance), current_delta_seconds(0.0f) {
}

AnimInstanceProxy::AnimInstanceProxy(const AnimInstanceProxy &) = default;
AnimInstanceProxy &AnimInstanceProxy::operator=(AnimInstanceProxy &&) = default;
AnimInstanceProxy &AnimInstanceProxy::operator=(const AnimInstanceProxy &) = default;
AnimInstanceProxy::~AnimInstanceProxy() = default;

AnimInstance *AnimInstanceProxy::GetAnimInstanceObject() const {
    return AnimInstanceObject;
}

void AnimInstanceProxy::Initialize(AnimInstance *InAnimInstance) {
    AnimInstanceObject = InAnimInstance;
    InitializeObjects(InAnimInstance);
    {
        root_node = InAnimInstance->GetAnimGraph()->GetRootNode();
    }
    InitializeRootNode(false);
}

void AnimInstanceProxy::InitializeObjects(AnimInstance *InAnimInstance) {
    // copy object instance
    skeletal_mesh = InAnimInstance->GetSkeletalMesh();
    skeleton = &(skeletal_mesh->GetRefSkeleton());
}

void AnimInstanceProxy::InitializeRootNode(bool bInDeferredRootNodeInitialization) {
    if (!bInDeferredRootNodeInitialization) {
        InitializeRootNode_WithRoot(root_node);
    }
}
void AnimInstanceProxy::InitializeRootNode_WithRoot(AnimNode *InRootNode) {

    if (InRootNode != nullptr) {

        AnimationUpdateSharedContext shared_context;
        AnimationInitializationContext init_context{this, &shared_context};

        if (InRootNode == root_node) {
            // TODO: Increment counter
            InRootNode->Initialize_AnyThread(init_context);
        } else {
            InRootNode->Initialize_AnyThread(init_context);
        }
    }
}

void AnimInstanceProxy::PreUpdate(AnimInstance *InAnimInstance, float DeltaSeconds) {
    InitializeObjects(InAnimInstance);
    current_delta_seconds = DeltaSeconds;
    // allocate blend weights and state machines
    // TODO: Collect Transform Here
    // GameThreadPreUpdateNodes => PreUpdate
}

void AnimInstanceProxy::RecalcRequiredBones(SkeletalMesh *InSkelMesh) {
    required_bones = InSkelMesh->GetSharedRequiredBones();
    // The First AnimInstance will init the required bones
    if (!required_bones->IsValid()) {
        required_bones->InitializeTo(InSkelMesh->RequiredBones, InSkelMesh);
        // Set Pose Override
    }
    bBoneCachesValid = false;
}

void AnimInstanceProxy::ResetAnimationCurves() {}
void AnimInstanceProxy::RecalcRequiredCurves() {}
void AnimInstanceProxy::UpdateCurvesForEvaluationContext() {}
void AnimInstanceProxy::UpdateCurvesPostEvaluation() {}

void AnimInstanceProxy::UpdateAnimation() {
    // SKR_LOG_INFO(u8"AnimInstanceProxy UpdateAnimation");

    AnimationUpdateSharedContext SharedContext;
    AnimationUpdateContext Context{this, current_delta_seconds, &SharedContext};

    // if valid - Context.SetNodeId
    UpdateAnimation_WithRoot(Context, root_node);
    // Tick Syncing
}

void AnimInstanceProxy::UpdateAnimation_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode) {
    // Layer?
    // Scope?
    if (!(GetAnimInstanceObject())->IsUpdateAnimationEnabled()) {
        return;
    }

    if (InRootNode == root_node) {
    } else {
        CacheBones_WithRoot(InRootNode);
    }

    // if updating root

    // update root
    {
        if (InRootNode == root_node) {
            // call override point
            UpdateAnimationNode(InContext);
        } else {
            UpdateAnimationNode_WithRoot(InContext, InRootNode);
        }
    }
}
void AnimInstanceProxy::CacheBones() {
    CacheBones_WithRoot(root_node);
}

void AnimInstanceProxy::CacheBones_WithRoot(AnimNode *InRootNode) {
    // root_node = InRootNode;
}

void AnimInstanceProxy::EvaluateAnimation(PoseContext &Output) {
    EvaluateAnimation_WithRoot(Output, root_node);
}

void AnimInstanceProxy::EvaluateAnimation_WithRoot(PoseContext &Output, AnimNode *InRootNode) {
    // TODO: CacheBones
    if (!Evaluate_WithRoot(Output, InRootNode)) {
        EvaluateAnimationNode_WithRoot(Output, InRootNode);
    }
}

void AnimInstanceProxy::UpdateAnimationNode(const AnimationUpdateContext &InContext) {
    UpdateAnimationNode_WithRoot(InContext, root_node);
}

void AnimInstanceProxy::UpdateAnimationNode_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode) {
    // TODO: Trace
    if (InRootNode != nullptr) {
        if (InRootNode == root_node) {
            // update counter
        }

        // Function calls
        // InitialUpdate
        // BecomeRelevant
        // Update
        InRootNode->Update_AnyThread(InContext);
        // PostGraphUpdate
    }
}

void AnimInstanceProxy::EvaluateAnimationNode(PoseContext &Output) {
    EvaluateAnimationNode_WithRoot(Output, root_node);
}

void AnimInstanceProxy::EvaluateAnimationNode_WithRoot(PoseContext &Output, AnimNode *InRootNode) {
    if (InRootNode != nullptr) {
        InRootNode->Evaluate_AnyThread(Output);
    } else {
        Output.ResetToRefPose();
    }
}

}// namespace rbc