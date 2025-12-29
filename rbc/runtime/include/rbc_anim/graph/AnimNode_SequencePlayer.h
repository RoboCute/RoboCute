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
    void Initialize_AnyThread(const AnimationInitializationContext &InContext) override;
    void Update_AnyThread(const AnimationUpdateContext &InContext) override;
    void Evaluate_AnyThread(PoseContext &Output) override;
    void NodeDebug() override;

    void Serialize(rbc::ArchiveWrite &w) override;
    void Deserialize(rbc::ArchiveRead &r) override;

public:
    // Interface
    void UpdateAssetPlayer(const AnimationUpdateContext &InContext);
    void CreateTickRecordForNode(const AnimationUpdateContext &InContext, AnimSequenceResource *InAnimSeqResource, bool bLooping, bool bIsEvaluator);

public:// temp public
    RC<AnimSequenceResource> anim_seq_resource;
    bool is_looping = true;
    float blend_weight = 0.0f;
    float internal_time_accumulator = 0.0f;
};

}// namespace rbc

RBC_RTTI(rbc::AnimNode_SequencePlayer)