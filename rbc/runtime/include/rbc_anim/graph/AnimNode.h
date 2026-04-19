#pragma once
#include "rbc_world/base_object.h"
#include "rbc_anim/types.h"
#include "rbc_anim/graph/AnimNodeContext.h"

namespace rbc {

struct AnimNode : public RCBase {
public:
    virtual ~AnimNode() {};
    AnimNode() = default;
    AnimNode(const AnimNode &) = delete;
    AnimNode &operator=(AnimNode &) = delete;

public:
    virtual void initialize_any_thread(const AnimationInitializationContext &in_context) = 0;
    virtual void update_any_thread(const AnimationUpdateContext &in_context) = 0;
    virtual void evaluate_any_thread(PoseContext &output) = 0;
    virtual void node_debug() = 0;

    // the special serialize and deserialize method for AnimNodes
    virtual void serialize(rbc::ArchiveWrite &w) = 0;
    virtual void deserialize(rbc::ArchiveRead &r) = 0;

    IndexType node_id = INVALID_INDEX;
};

struct PoseLink : RCBase {
    friend struct Serialize<PoseLink>;
public:
    IndexType linked_node_id = INVALID_INDEX;// Serialized Link ID from Graph
protected:
    AnimNode *_linked_node = nullptr;

public:
    void attempt_relink(const AnimationBaseContext &in_context);
    AnimNode *get_linked_node();
    void initialize(const AnimationInitializationContext &in_context);
    void update(const AnimationUpdateContext &in_context);
    void evaluate(PoseContext &output);
};

}// namespace rbc

RBC_RTTI(rbc::AnimNode)
RBC_RTTI(rbc::PoseLink)

template<>
struct RBC_RUNTIME_API rbc::Serialize<rbc::PoseLink> {
    static bool write(rbc::ArchiveWrite &w, const rbc::PoseLink &v);
    static bool read(rbc::ArchiveRead &r, rbc::PoseLink &v);
};