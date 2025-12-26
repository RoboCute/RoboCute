#pragma once
#include "rbc_anim/graph/AnimNodeContext.h"
#include "rbc_anim/graph/AnimNode.h"

namespace rbc {

struct RBC_RUNTIME_API AnimNode_Root : public AnimNode {
    void Initialize_AnyThread(const AnimationInitializationContext &InContext) override;
    void Update_AnyThread(const AnimationUpdateContext &InContext) override;
    void Evaluate_AnyThread(PoseContext &Output) override;
    void NodeDebug() override;
public:
    PoseLink result;
};

}// namespace rbc