#pragma once

#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/graph/AnimNodeContext.h"
#include "rbc_world/resources/anim_sequence.h"

namespace rbc {

struct RBC_RUNTIME_API AnimNode_SequencePlayer : public AnimNode {
public:
    AnimNode_SequencePlayer() = default;
    virtual ~AnimNode_SequencePlayer() {}

public:
    // Node Interface
    void initialize_any_thread(const AnimationInitializationContext &in_context) override;
    void update_any_thread(const AnimationUpdateContext &in_context) override;
    void evaluate_any_thread(PoseContext &output) override;
    void node_debug() override;

    void serialize(rbc::ArchiveWrite &w) override;
    void deserialize(rbc::ArchiveRead &r) override;

public:
    // Interface
    void update_asset_player(const AnimationUpdateContext &in_context);
    void create_tick_record_for_node(const AnimationUpdateContext &in_context, world::AnimSequenceResource *in_anim_seq_resource, bool b_looping, bool b_is_evaluator);

public:// temp public
    RC<world::AnimSequenceResource> anim_seq_resource;
    bool is_looping = true;
    float blend_weight = 0.0f;
    float internal_time_accumulator = 0.0f;
};

}// namespace rbc

RBC_RTTI(rbc::AnimNode_SequencePlayer)