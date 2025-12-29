#include "rbc_anim/graph/AnimNode_Root.h"

namespace rbc {

void AnimNode_Root::Initialize_AnyThread(const AnimationInitializationContext &InContext) {
    LUISA_INFO("Initialize AnimNode_Root");
    result.Initialize(InContext);
}

void AnimNode_Root::Update_AnyThread(const AnimationUpdateContext &InContext) {
    LUISA_INFO("Updating AnimNode_Root");
    result.Update(InContext);
}
void AnimNode_Root::Evaluate_AnyThread(PoseContext &Output) {
    LUISA_INFO("Evaluating AnimNode_Root");
    result.Evaluate(Output);
}
void AnimNode_Root::NodeDebug() {
    LUISA_INFO("Debugging AnimNode_Root");
}

}// namespace rbc