#pragma once
#include "rbc_world/base_object.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include "rbc_anim/bone_pose.h"
#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/graph/AnimGraph.h"

namespace rbc {
struct AnimInstance;
struct AnimInstanceProxy;
struct AnimGraphResource;
struct AnimationUpdateContext;
struct PoseContext;
struct BoneContainer;
struct SkeletalMesh;
}// namespace rbc

namespace rbc {

struct AnimInstanceUpdateContext {
    AnimInstance *anim_instance;
};

struct ParallelEvaluationData {
    // Curve
    CompactPose &OutPose;
    // OutAttribute
};

// AnimInstance
// AnimGraph资产对应的动态数据结构
// 遍历并执行动画节点的入口，执行绑定的AnimGraph
// 可能会被复制到多个角色持有相同的动作
struct RBC_RUNTIME_API AnimInstance : world::BaseObject {

public:
    void InitAnimInstance(RC<AnimGraphResource> &InAnimGraph);

    void BindSkelMesh(SkeletalMesh *InSkelMesh);
    SkeletalMesh *GetSkeletalMesh() const;
    AnimGraph *GetAnimGraph();

    void InitializeAnimation();

    virtual AnimInstanceProxy *CreateAnimInstanceProxy();
    virtual void DestroyAnimInstanceProxy(AnimInstanceProxy *InProxy);
    enum class EUpdateAnimationFlag : uint8_t {
        ForceParallelUpdate,
        Default
    };

    void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion, EUpdateAnimationFlag UpdateFlag);
    void PreUpdateAnimation(float DeltaSeconds);
    void PostUpdateAnimation();
    void ParallelEvaluateAnimation(bool bForceRefPose, const SkeletalMesh *InSkeletalMesh, ParallelEvaluationData &OutData);

    // 存在Graph依然需要eval但是不需要update的情况，设置flag来disble update
    void EnableUpdateAnimation(bool bEnable) {
        bUpdateAnimationEnabled = bEnable;
    }
    bool IsUpdateAnimationEnabled() const { return bUpdateAnimationEnabled; }

    template<typename T>
    static T *GetProxyOnGameThreadStatic(AnimInstance *InAnimInstance) {
        if (InAnimInstance) {
            if (InAnimInstance->proxy == nullptr) {
                InAnimInstance->proxy = InAnimInstance->CreateAnimInstanceProxy();
            }
            return static_cast<T *>(InAnimInstance->proxy);
        }
        return nullptr;
    }

    template<typename T>
    T &GetProxyOnGameThread() {
        return *GetProxyOnGameThreadStatic<T>(this);
    }

    template<typename T>
    const T &GetProxyOnGameThread() const {
        if (proxy == nullptr) {
            proxy = const_cast<AnimInstance *>(this)->CreateAnimInstanceProxy();
        }
        return *static_cast<const T *>(proxy);
    }

    template<typename T>
    RBC_FORCEINLINE T &GetProxyOnAnyThread() {
        if (proxy == nullptr) {
            proxy = CreateAnimInstanceProxy();
        }
        return *static_cast<T *>(proxy);
    }

    // AnimInstance不一定需要用到整个Skeleton的全部骨骼，只需要部分
    void RecalcRequiredBones();
    void RecalcRequiredCurves();

    bool NeedsUpdate();
    void ParallelUpdateAnimation();

private:
    RC<AnimGraphResource> anim_graph;
    SkeletalMesh *skel_mesh;
    mutable AnimInstanceProxy *proxy = nullptr;
    bool bUpdateAnimationEnabled = true;
    bool bNeedsUpdate = false;
};

struct RBC_RUNTIME_API AnimInstanceProxy {
public:
    AnimInstanceProxy();
    AnimInstanceProxy(const AnimInstanceProxy &);
    AnimInstanceProxy &operator=(AnimInstanceProxy &&);
    AnimInstanceProxy &operator=(const AnimInstanceProxy &);
    virtual ~AnimInstanceProxy();

    explicit AnimInstanceProxy(AnimInstance *InAnimInstance);

public:// LifeCycle
    AnimInstance *GetAnimInstanceObject() const;
    void Initialize(AnimInstance *InAnimInstance);
    void InitializeObjects(AnimInstance *InAnimInstance);
    void InitializeRootNode(bool bInDeferredRootNodeInitialization);
    void InitializeRootNode_WithRoot(AnimNode *InRootNode);
    void PreUpdate(AnimInstance *InAnimInstance, float DeltaTimeSeconds);
    // overwrite point for evaluate anim
    virtual bool Evaluate(PoseContext &Output) { return false; }
    // overwrite point for evaluate with root
    virtual bool Evaluate_WithRoot(PoseContext &Output, AnimNode *InRootNode) {
        return Evaluate(Output);
    }

public:// Utilities
    void RecalcRequiredBones(SkeletalMesh *InSkelMesh);
    void RecalcRequiredCurves();
    // Curves
    void ResetAnimationCurves();

    void UpdateCurvesForEvaluationContext();
    void UpdateCurvesPostEvaluation();

public:                    // Core Function
    void UpdateAnimation();// Entrance for Update Phase
    void UpdateAnimation_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode);
    void EvaluateAnimation(PoseContext &Output);// Entrance for Evaluate Phase
    void EvaluateAnimation_WithRoot(PoseContext &Output, AnimNode *InRootNode);

    void CacheBones();
    void CacheBones_WithRoot(AnimNode *InRootNode);
    void UpdateAnimationNode(const AnimationUpdateContext &InContext);
    void UpdateAnimationNode_WithRoot(const AnimationUpdateContext &InContext, AnimNode *InRootNode);
    void EvaluateAnimationNode(PoseContext &Output);
    void EvaluateAnimationNode_WithRoot(PoseContext &Output, AnimNode *InRootNode);

public:
    // Get & Set
    BoneContainer &GetRequiredBones() {
        return *required_bones;
    }
    [[nodiscard]] const BoneContainer &GetRequiredBones() const {
        return *required_bones;
    }

private:
    mutable AnimInstance *AnimInstanceObject;

    const ReferenceSkeleton *skeleton;
    SkeletalMesh *skeletal_mesh;
    AnimGraph *anim_graph;
    AnimNode *root_node;// The Root Node Entry for this graph

    AnimInstanceProxy *MainInstanceProxy;
    // Bone Indicies Required for this Frame
    luisa::shared_ptr<BoneContainer> required_bones;

    // ==================== Sampling State =====================
    float current_delta_seconds;// The last timer passed via PreUpdate
    bool bUpdatingRoot;         // scope guard to prevent duplicate perform
    bool bBoneCachesValid = false;
    // float CurrentTimeDilation;
    // =========================================================
    // Buffers
};

}// namespace rbc
