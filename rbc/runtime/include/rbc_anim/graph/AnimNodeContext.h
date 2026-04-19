#pragma once
#include "rbc_anim/types.h"
#include "rbc_anim/bone_pose.h"
#include "rbc_anim/anim_record.h"

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
    explicit AnimationBaseContext(AnimInstanceProxy *in_anim_instance_proxy, AnimationUpdateSharedContext *in_share_context = nullptr);

public:
    // Get Animaiton Class Type
    AnimInstance *get_anim_instance_object() const;
    IndexType get_current_node_id() const { return _current_node_id; }
    IndexType get_previous_node_id() const { return _previous_node_id; }
    AnimationUpdateSharedContext *get_shared_context() const { return shared_context; }

protected:
    bool _is_active = false;
    IndexType _current_node_id = INVALID_INDEX;
    IndexType _previous_node_id = INVALID_INDEX;
};

struct AnimationInitializationContext : public AnimationBaseContext {
public:
    AnimationInitializationContext(AnimInstanceProxy *in_anim_instance_proxy, AnimationUpdateSharedContext *in_share_context = nullptr)
        : AnimationBaseContext(in_anim_instance_proxy, in_share_context) {
    }
};

struct AnimationUpdateContext : public AnimationBaseContext {
private:
    // Core Context
    float _current_weight;
    float _root_motion_weight_modifier;
    float _delta_time;

public:
    // Default Ctor
    AnimationUpdateContext(AnimInstanceProxy *in_anim_instance_proxy = nullptr)
        : AnimationBaseContext(in_anim_instance_proxy), _current_weight(1.0f), _root_motion_weight_modifier(1.0f), _delta_time(0.0f) {
    }
    // Most Commonly used Ctor
    AnimationUpdateContext(AnimInstanceProxy *in_anim_instance_proxy, float in_delta_time, AnimationUpdateSharedContext *in_shared_context)
        : AnimationBaseContext(in_anim_instance_proxy, in_shared_context), _current_weight(1.0f), _root_motion_weight_modifier(1.0f), _delta_time(in_delta_time) {
    }
    // Special Copy
    AnimationUpdateContext(const AnimationUpdateContext &copy, AnimInstanceProxy *in_another_proxy)
        : AnimationBaseContext(in_another_proxy, copy.shared_context), _current_weight(copy._current_weight), _root_motion_weight_modifier(copy._root_motion_weight_modifier), _delta_time(copy._delta_time) {
        _current_node_id = copy._current_node_id;
        _previous_node_id = copy._previous_node_id;
    }

public:
    AnimationUpdateContext with_other_proxy(AnimInstanceProxy *in_anim_instance_proxy) const {
        AnimationUpdateContext result(*this, in_anim_instance_proxy);
        return result;
    }
    AnimationUpdateContext with_other_shared_context(AnimationUpdateSharedContext *in_shared_context) const {
        AnimationUpdateContext result(*this);// Default Copy
        result.shared_context = in_shared_context;
        return result;
    }

public:
    // get & set
    float get_delta_time() const;
    float get_final_blend_weight() const;
};

// Context for Actual Evaluate Pose
struct PoseContext : public AnimationBaseContext {
public:
    CompactPose pose;
    // Curve
    // CustomAttribute
    bool expects_additive_pose;

public:
    PoseContext(AnimInstanceProxy *in_anim_instance_proxy, bool in_expects_additive_pose = false)
        : AnimationBaseContext(in_anim_instance_proxy), expects_additive_pose(in_expects_additive_pose) {
        initialize_impl(in_anim_instance_proxy);
    }

    void initialize_impl(AnimInstanceProxy *in_anim_instance_proxy);

public:
    // get & set
    bool get_expects_additive_pose() const { return expects_additive_pose; }
    void reset_to_ref_pose();
};

struct AnimationPoseData {
public:
    AnimationPoseData(PoseContext &in_pose_context);
    // Slot
    AnimationPoseData(CompactPose &in_pose);
    // No Default and Move Constructor
    AnimationPoseData() = delete;
    AnimationPoseData &operator=(AnimationPoseData &&other) = delete;

public:
    // Getter
    const CompactPose &get_pose() const;
    CompactPose &get_pose();

protected:
    CompactPose &_pose;
};

struct AnimExtractContext {
    double current_time;// The Position in Sequence
    bool looping;
    bool extract_root_motion;
    DeltaTimeRecord delta_time_record;
    luisa::vector<bool> bones_required;
};

}// namespace rbc