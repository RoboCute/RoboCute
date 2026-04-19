
#include "rbc_core/memory.h"
// #include "rbc_world/base_object.h"  // Unused include
#include "rbc_anim/bone_container.h"
#include "rbc_anim/anim_instance.h"
#include "rbc_anim/graph/AnimGraph.h"
#include "rbc_world/resources/anim_graph.h"
#include "rbc_anim/skeletal_mesh.h"

namespace rbc {

void AnimInstance::InitAnimInstance(const RC<world::AnimGraphResource> &InAnimGraph) {
    LUISA_INFO("Init AnimInstance");
    _anim_graph = InAnimGraph;
}

void AnimInstance::BindSkelMesh(SkeletalMesh *InSkelMesh) {
    LUISA_INFO("[AnimInstance] Binding SkelMesh");
    _skel_mesh = InSkelMesh;
}
SkeletalMesh *AnimInstance::GetSkeletalMesh() const {
    return _skel_mesh;
}
AnimGraph *AnimInstance::GetAnimGraph() {
    return &(_anim_graph->graph());
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

bool AnimInstance::NeedsUpdate() const {
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
    _b_needs_update = true;
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

    OutData.OutPose.set_bone_container(&AnimProxy.GetRequiredBones());

    if (!bForceRefPose) {
        PoseContext eval_context(&AnimProxy);
        eval_context.reset_to_ref_pose();
        // Run AnimGraph
        AnimProxy.EvaluateAnimation(eval_context);
        OutData.OutPose.copy_bones_from(eval_context.pose);
    } else {
        OutData.OutPose.reset_to_ref_pose();
    }
}

}// namespace rbc

namespace rbc {

AnimInstanceProxy::AnimInstanceProxy()
    : _anim_instance_object(nullptr), _skeleton(nullptr), _skeletal_mesh(nullptr), _anim_graph(nullptr), _root_node(nullptr), _main_instance_proxy(nullptr), _current_delta_seconds(0.0f), _b_updating_root(false) {
}

AnimInstanceProxy::AnimInstanceProxy(AnimInstance *InAnimInstance)
    : _anim_instance_object(InAnimInstance), _skeleton(nullptr), _skeletal_mesh(nullptr), _anim_graph(nullptr), _root_node(nullptr), _main_instance_proxy(nullptr), _current_delta_seconds(0.0f), _b_updating_root(false) {
}

AnimInstanceProxy::AnimInstanceProxy(const AnimInstanceProxy &) = default;
AnimInstanceProxy &AnimInstanceProxy::operator=(AnimInstanceProxy &&) = default;
AnimInstanceProxy &AnimInstanceProxy::operator=(const AnimInstanceProxy &) = default;
AnimInstanceProxy::~AnimInstanceProxy() = default;

AnimInstance *AnimInstanceProxy::GetAnimInstanceObject() const {
    return _anim_instance_object;
}

void AnimInstanceProxy::Initialize(AnimInstance *InAnimInstance) {
    _anim_instance_object = InAnimInstance;
    InitializeObjects(InAnimInstance);
    {
        _root_node = InAnimInstance->GetAnimGraph()->get_root_node();
    }
    InitializeRootNode(false);
}

void AnimInstanceProxy::InitializeObjects(const AnimInstance *InAnimInstance) {
    // copy object instance
    _skeletal_mesh = InAnimInstance->GetSkeletalMesh();
    _skeleton = &(_skeletal_mesh->GetRefSkeleton());
}

void AnimInstanceProxy::InitializeRootNode(bool bInDeferredRootNodeInitialization) {
    if (!bInDeferredRootNodeInitialization) {
        InitializeRootNode_WithRoot(_root_node);
    }
}
void AnimInstanceProxy::InitializeRootNode_WithRoot(AnimNode *InRootNode) {

    if (InRootNode != nullptr) {

        AnimationUpdateSharedContext shared_context;
        AnimationInitializationContext init_context{this, &shared_context};

        // Initialize the node regardless of whether it's the root node
        InRootNode->initialize_any_thread(init_context);
    }
}

void AnimInstanceProxy::PreUpdate(const AnimInstance *InAnimInstance, float DeltaTimeSeconds) {
    InitializeObjects(InAnimInstance);
    _current_delta_seconds = DeltaTimeSeconds;
    // allocate blend weights and state machines
    // TODO: Collect Transform Here
    // GameThreadPreUpdateNodes => PreUpdate
}

void AnimInstanceProxy::RecalcRequiredBones(SkeletalMesh *InSkelMesh) {
    _required_bones = InSkelMesh->GetSharedRequiredBones();
    // The First AnimInstance will init the required bones
    if (!_required_bones->is_valid()) {
        _required_bones->initialize_to(InSkelMesh->RequiredBones, InSkelMesh);
        // Set Pose Override
    }
    _b_bone_caches_valid = false;
}

void AnimInstanceProxy::ResetAnimationCurves() {}
void AnimInstanceProxy::RecalcRequiredCurves() {}
void AnimInstanceProxy::UpdateCurvesForEvaluationContext() {}
void AnimInstanceProxy::UpdateCurvesPostEvaluation() {}

void AnimInstanceProxy::UpdateAnimation() {
    // SKR_LOG_INFO(u8"AnimInstanceProxy UpdateAnimation");

    AnimationUpdateSharedContext SharedContext;
    AnimationUpdateContext Context{this, _current_delta_seconds, &SharedContext};

    // if valid - Context.SetNodeId
    UpdateAnimation_WithRoot(Context, _root_node);
    // Tick Syncing
}

void AnimInstanceProxy::UpdateAnimation_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode) {
    // Layer?
    // Scope?
    if (!(GetAnimInstanceObject())->IsUpdateAnimationEnabled()) {
        return;
    }

    if (InRootNode == _root_node) {
    } else {
        CacheBones_WithRoot(InRootNode);
    }

    // if updating root

    // update root
    {
        if (InRootNode == _root_node) {
            // call override point
            UpdateAnimationNode(InContext);
        } else {
            UpdateAnimationNode_WithRoot(InContext, InRootNode);
        }
    }
}
void AnimInstanceProxy::CacheBones() {
    CacheBones_WithRoot(_root_node);
}

void AnimInstanceProxy::CacheBones_WithRoot(AnimNode *InRootNode) {
    // root_node = InRootNode;
}

void AnimInstanceProxy::EvaluateAnimation(PoseContext &Output) {
    EvaluateAnimation_WithRoot(Output, _root_node);
}

void AnimInstanceProxy::EvaluateAnimation_WithRoot(PoseContext &Output, AnimNode *InRootNode) {
    // TODO: CacheBones
    if (!Evaluate_WithRoot(Output, InRootNode)) {
        EvaluateAnimationNode_WithRoot(Output, InRootNode);
    }
}

void AnimInstanceProxy::UpdateAnimationNode(const AnimationUpdateContext &InContext) {
    UpdateAnimationNode_WithRoot(InContext, _root_node);
}

void AnimInstanceProxy::UpdateAnimationNode_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode) {
    // TODO: Trace
    if (InRootNode != nullptr) {
        if (InRootNode == _root_node) {
            // update counter
        }

        // Function calls
        // InitialUpdate
        // BecomeRelevant
        // Update
        InRootNode->update_any_thread(InContext);
        // PostGraphUpdate
    }
}

void AnimInstanceProxy::EvaluateAnimationNode(PoseContext &Output) {
    EvaluateAnimationNode_WithRoot(Output, _root_node);
}

void AnimInstanceProxy::EvaluateAnimationNode_WithRoot(PoseContext &Output, AnimNode *InRootNode) {
    if (InRootNode != nullptr) {
        InRootNode->evaluate_any_thread(Output);
    } else {
        Output.reset_to_ref_pose();
    }
}

}// namespace rbc