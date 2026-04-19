#pragma once
#include "rbc_anim/graph/AnimNodeContext.h"
#include "rbc_anim/graph/AnimNode.h"

namespace rbc {

struct RBC_RUNTIME_API AnimNode_Root : public AnimNode {
public:
    AnimNode_Root() = default;
    virtual ~AnimNode_Root() {}

public:
    void initialize_any_thread(const AnimationInitializationContext &in_context) override;
    void update_any_thread(const AnimationUpdateContext &in_context) override;
    void evaluate_any_thread(PoseContext &output) override;
    void node_debug() override;

    void serialize(rbc::ArchiveWrite &w) override;
    void deserialize(rbc::ArchiveRead &r) override;

public:
    PoseLink result;
};

}// namespace rbc

RBC_RTTI(rbc::AnimNode_Root)