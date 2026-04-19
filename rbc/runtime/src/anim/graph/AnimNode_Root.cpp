#include "rbc_anim/graph/AnimNode_Root.h"

namespace rbc {

void AnimNode_Root::initialize_any_thread(const AnimationInitializationContext &in_context) {
    LUISA_INFO("Initialize AnimNode_Root");
    result.initialize(in_context);
}

void AnimNode_Root::update_any_thread(const AnimationUpdateContext &in_context) {
    // LUISA_INFO("Updating AnimNode_Root");
    result.update(in_context);
}

void AnimNode_Root::evaluate_any_thread(PoseContext &output) {
    // LUISA_INFO("Evaluating AnimNode_Root");
    result.evaluate(output);
}

void AnimNode_Root::node_debug() {
    LUISA_INFO("Debugging AnimNode_Root");
}

void AnimNode_Root::serialize(rbc::ArchiveWrite &w) {
    w.value<rbc::PoseLink>(result, "result");
}

void AnimNode_Root::deserialize(rbc::ArchiveRead &r) {
    r.value<rbc::PoseLink>(result, "result");
}

}// namespace rbc