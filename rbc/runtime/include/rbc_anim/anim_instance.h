#pragma once
#include "rbc_world/base_object.h"
#include "rbc_anim/asset/reference_skeleton.h"
#include "rbc_anim/bone_pose.h"
#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/graph/AnimGraph.h"
namespace rbc::world {
struct AnimGraphResource;
}// namespace rbc::world
namespace rbc {
struct AnimInstance;
struct AnimInstanceProxy;
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
struct RBC_RUNTIME_API AnimInstance : RCBase {

public:
    void InitAnimInstance(const RC<world::AnimGraphResource> &InAnimGraph);

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
        _b_update_animation_enabled = bEnable;
    }
    bool IsUpdateAnimationEnabled() const { return _b_update_animation_enabled; }

    template<typename T>
    static T *GetProxyOnGameThreadStatic(AnimInstance *InAnimInstance) {
        if (InAnimInstance) {
            if (InAnimInstance->_proxy == nullptr) {
                InAnimInstance->_proxy = InAnimInstance->CreateAnimInstanceProxy();
            }
            return static_cast<T *>(InAnimInstance->_proxy);
        }
        return nullptr;
    }

    template<typename T>
    T &GetProxyOnGameThread() {
        return *GetProxyOnGameThreadStatic<T>(this);
    }

    template<typename T>
    const T &GetProxyOnGameThread() const {
        if (_proxy == nullptr) {
            _proxy = const_cast<AnimInstance *>(this)->CreateAnimInstanceProxy();
        }
        return *static_cast<const T *>(_proxy);
    }

    template<typename T>
    RBC_FORCEINLINE T &GetProxyOnAnyThread() {
        if (_proxy == nullptr) {
            _proxy = CreateAnimInstanceProxy();
        }
        return *static_cast<T *>(_proxy);
    }

    // AnimInstance不一定需要用到整个Skeleton的全部骨骼，只需要部分
    void RecalcRequiredBones();
    void RecalcRequiredCurves();

    bool NeedsUpdate() const;
    void ParallelUpdateAnimation();

private:
    RC<world::AnimGraphResource> _anim_graph;
    SkeletalMesh *_skel_mesh;
    mutable AnimInstanceProxy *_proxy = nullptr;
    bool _b_update_animation_enabled = true;
    bool _b_needs_update = false;
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
    void InitializeObjects(const AnimInstance *InAnimInstance);
    void InitializeRootNode(bool bInDeferredRootNodeInitialization);
    void InitializeRootNode_WithRoot(AnimNode *InRootNode);
    void PreUpdate(const AnimInstance *InAnimInstance, float DeltaTimeSeconds);
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
        return *_required_bones;
    }
    [[nodiscard]] const BoneContainer &GetRequiredBones() const {
        return *_required_bones;
    }

private:
    mutable AnimInstance *_anim_instance_object;

    const ReferenceSkeleton *_skeleton;
    SkeletalMesh *_skeletal_mesh;
    [[maybe_unused]] AnimGraph *_anim_graph;
    AnimNode *_root_node;// The Root Node Entry for this graph

    [[maybe_unused]] AnimInstanceProxy *_main_instance_proxy;
    // Bone Indicies Required for this Frame
    luisa::shared_ptr<BoneContainer> _required_bones;

    // ==================== Sampling State =====================
    float _current_delta_seconds;        // The last timer passed via PreUpdate
    [[maybe_unused]] bool _b_updating_root;// scope guard to prevent duplicate perform
    bool _b_bone_caches_valid = false;
    // float CurrentTimeDilation;
    // =========================================================
    // Buffers
};

}// namespace rbc
