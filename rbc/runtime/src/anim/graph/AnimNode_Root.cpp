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

void AnimNode_Root::Serialize(rbc::ArchiveWrite &w) {
    w.start_object();
    rbc::Serialize<rbc::PoseLink>::write(w, result);
    w.end_object("result");
}
void AnimNode_Root::Deserialize(rbc::ArchiveRead &r) {
    r.start_object("result");
    rbc::Serialize<rbc::PoseLink>::read(r, result);
    r.end_scope();
}

}// namespace rbc