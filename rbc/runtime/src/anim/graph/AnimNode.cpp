#include "rbc_anim/graph/AnimNode.h"
#include "rbc_anim/graph/AnimGraph.h"
#include "rbc_anim/anim_instance.h"
#include "rbc_world/type_register.h"
#include "rbc_world/resources/anim_graph.h"

namespace rbc {

void PoseLink::attempt_relink(const AnimationBaseContext &in_context) {

    if (_linked_node == nullptr && linked_node_id != INVALID_INDEX) {
        LUISA_INFO("Attempt Relinking...");
        _linked_node = in_context.get_anim_instance_object()->GetAnimGraph()->nodes[linked_node_id].get();
    }
}

AnimNode *PoseLink::get_linked_node() {
    return _linked_node;
}

void PoseLink::initialize(const AnimationInitializationContext &in_context) {
    LUISA_INFO("Initializing Through PoseLink for LinkedNode {}", linked_node_id);

    attempt_relink(in_context);

    if (_linked_node != nullptr) {
        AnimationInitializationContext init_ctx(in_context);
        _linked_node->initialize_any_thread(init_ctx);
    }
}

void PoseLink::update(const AnimationUpdateContext &in_context) {
    if (_linked_node) {
        _linked_node->update_any_thread(in_context);
    }
}

void PoseLink::evaluate(PoseContext &output) {
    // LUISA_INFO("Evaluating Through PoseLink for LinkedNode {}", linked_node_id);
    if (_linked_node != nullptr) {
        _linked_node->evaluate_any_thread(output);
    } else {
        output.reset_to_ref_pose();
    }
    // Detect Valid Output
}

}// namespace rbc

bool rbc::Serialize<rbc::PoseLink>::write(rbc::ArchiveWrite &w, const rbc::PoseLink &v) {
    w.value(v.linked_node_id, "linked_node_id");
    return true;
}
bool rbc::Serialize<rbc::PoseLink>::read(rbc::ArchiveRead &r, rbc::PoseLink &v) {
    r.start_object();
    r.value(v.linked_node_id, "linked_node_id");
    r.end_scope();
    return true;
}