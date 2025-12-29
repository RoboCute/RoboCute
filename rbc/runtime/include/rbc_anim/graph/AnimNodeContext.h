#pragma once
#include "rbc_anim/types.h"
#include "rbc_anim/bone_pose.h"

namespace rbc {
struct AnimInstance;
struct AnimInstanceProxy;
}// namespace rbc

namespace rbc {

struct AnimationUpdateSharedContext {
    AnimationUpdateSharedContext() = default;
    // No Copy
    AnimationUpdateSharedContext(AnimationUpdateSharedContext &) = delete;
    AnimationUpdateSharedContext &operator=(const AnimationUpdateSharedContext &) = delete;
};

// base context for update & evaluation task
struct AnimationBaseContext {
public:
    AnimInstanceProxy *anim_instance_proxy;
    AnimationUpdateSharedContext *shared_context;

public:
    AnimationBaseContext();
    AnimationBaseContext(const AnimationBaseContext &) = default;
    AnimationBaseContext(AnimationBaseContext &&) = default;
    AnimationBaseContext &operator=(const AnimationBaseContext &) = default;
    AnimationBaseContext &operator=(AnimationBaseContext &&) = default;

protected:
    explicit AnimationBaseContext(AnimInstanceProxy *InAnimInstanceProxy, AnimationUpdateSharedContext *InShareContext = nullptr);

public:
    // Get Animaiton Class Type
    AnimInstance *GetAnimInstanceObject() const;
    IndexType GetCurrentNodeId() const { return current_node_id; }
    IndexType GetPreviousNodeId() const { return previous_node_id; }
    AnimationUpdateSharedContext *GetSharedContext() const { return shared_context; }

protected:
    bool is_active = false;
    IndexType current_node_id = INVALID_INDEX;
    IndexType previous_node_id = INVALID_INDEX;
};

struct AnimationInitializationContext : public AnimationBaseContext {
public:
    AnimationInitializationContext(AnimInstanceProxy *InAnimInstanceProxy, AnimationUpdateSharedContext *InShareContext = nullptr)
        : AnimationBaseContext(InAnimInstanceProxy, InShareContext) {
    }
};

struct AnimationUpdateContext : public AnimationBaseContext {
private:
    // Core Context
    float current_weight;
    float root_motion_weight_modifier;
    float delta_time;

public:
    // Default Ctor
    AnimationUpdateContext(AnimInstanceProxy *InAnimInstanceProxy = nullptr)
        : AnimationBaseContext(InAnimInstanceProxy), current_weight(1.0f), root_motion_weight_modifier(1.0f), delta_time(0.0f) {
    }
    // Most Commonly used Ctor
    AnimationUpdateContext(AnimInstanceProxy *InAnimInstanceProxy, float InDeltaTime, AnimationUpdateSharedContext *InSharedContext)
        : AnimationBaseContext(InAnimInstanceProxy, InSharedContext), delta_time(InDeltaTime), current_weight(1.0f), root_motion_weight_modifier(1.0f) {
    }
    // Special Copy
    AnimationUpdateContext(const AnimationUpdateContext &Copy, AnimInstanceProxy *InAnotherProxy)
        : AnimationBaseContext(InAnotherProxy, Copy.shared_context), current_weight(Copy.current_weight), root_motion_weight_modifier(Copy.root_motion_weight_modifier), delta_time(Copy.delta_time) {
        current_node_id = Copy.current_node_id;
        previous_node_id = Copy.previous_node_id;
    }

public:
    AnimationUpdateContext WithOtherProxy(AnimInstanceProxy *InAnimInstanceProxy) const {
        AnimationUpdateContext Result(*this, InAnimInstanceProxy);
        return Result;
    }
    AnimationUpdateContext WithOtherSharedContext(AnimationUpdateSharedContext *InSharedContext) const {
        AnimationUpdateContext Result(*this);// Default Copy
        Result.shared_context = InSharedContext;
        return Result;
    }

public:
    // get & set
    float GetDeltaTime() const;
    float GetFinalBlendWeight() const;
};

// Context for Actual Evaluate Pose
struct PoseContext : public AnimationBaseContext {
public:
    CompactPose Pose;
    // Curve
    // CustomAttribute
    bool bExpectsAdditivePose;

public:
    PoseContext(AnimInstanceProxy *InAnimInstanceProxy, bool bInExpectsAdditivePose = false)
        : AnimationBaseContext(InAnimInstanceProxy), bExpectsAdditivePose(bInExpectsAdditivePose) {
        InitializeImpl(InAnimInstanceProxy);
    }

    void InitializeImpl(AnimInstanceProxy *InAnimInstanceProxy);

public:
    // get & set
    bool ExpectsAdditivePose() const { return bExpectsAdditivePose; }
    void ResetToRefPose();
};

}// namespace rbc