#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/graph/AnimGraph.h"
#include "rbc_anim/anim_instance.h"
#include "rbc_world/type_register.h"

namespace rbc {

void PoseLink::AttemptRelink(const AnimationBaseContext &InContext) {

    if (LinkedNode == nullptr && LinkedNodeID != INVALID_INDEX) {
        LUISA_INFO("Attempt Relinking...");
        LinkedNode = InContext.GetAnimInstanceObject()->GetAnimGraph()->nodes[LinkedNodeID].get();
    }
}

AnimNode *PoseLink::GetLinkedNode() {
    return LinkedNode;
}

void PoseLink::Initialize(const AnimationInitializationContext &InContext) {
    LUISA_INFO("Initializing Through PoseLink for LinkedNode {}", LinkedNodeID);

    AttemptRelink(InContext);

    if (LinkedNode != nullptr) {
        AnimationInitializationContext init_ctx(InContext);
        LinkedNode->Initialize_AnyThread(init_ctx);
    }
}

void PoseLink::Update(const AnimationUpdateContext &InContext) {
    if (LinkedNode) {
        LinkedNode->Update_AnyThread(InContext);
    }
}

void PoseLink::Evaluate(PoseContext &Output) {
    LUISA_INFO("Evaluating Through PoseLink for LinkedNode {}", LinkedNodeID);
    if (LinkedNode != nullptr) {
        LinkedNode->Evaluate_AnyThread(Output);
    } else {
        Output.ResetToRefPose();
    }
    // Detect Valid Output
}

}// namespace rbc

bool rbc::Serialize<rbc::PoseLink>::write(rbc::ArchiveWrite &w, const rbc::PoseLink &v) {
    w.value(v.LinkedNodeID, "LinkedNodeID");
    return true;
}
bool rbc::Serialize<rbc::PoseLink>::read(rbc::ArchiveRead &r, rbc::PoseLink &v) {
    r.start_object();
    r.value(v.LinkedNodeID, "LinkedNodeID");
    r.end_scope();
    return true;
}