#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/anim_instance.h"
#include "rbc_world/type_register.h"

namespace rbc {

void PoseLink::AttemptRelink(const AnimationBaseContext &InContext) {
    if (LinkedNode == nullptr && LinkedNodeID != INVALID_INDEX) {
        // LinkedNode = InContext.GetAnimInstanceObject()->GetAnimGraph()->nodes[LinkedNodeID];
    }
}

AnimNode *PoseLink::GetLinkedNode() {
    return LinkedNode;
}

void PoseLink::Initialize(const AnimationInitializationContext &InContext) {
    AttemptRelink(InContext);
    if (LinkedNode != nullptr) {
        AnimationInitializationContext init_ctx(InContext);
        // Set Link ID
        LinkedNode->Initialize_AnyThread(init_ctx);
    }
}

void PoseLink::Update(const AnimationUpdateContext &InContext) {
    if (LinkedNode) {
        LinkedNode->Update_AnyThread(InContext);
    }
}

void PoseLink::Evaluate(PoseContext &Output) {
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
}
bool rbc::Serialize<rbc::PoseLink>::read(rbc::ArchiveRead &r, rbc::PoseLink &v) {
    r.start_object();
    r.value(v.LinkedNodeID, "LinkedNodeID");
    r.end_scope();
}