#pragma once
#include "rbc_world/base_object.h"
#include "rbc_anim/types.h"
#include "rbc_anim/graph/AnimNodeContext.h"

namespace rbc {

struct AnimNode : public RCBase {
public:
    virtual ~AnimNode() {};
    AnimNode::AnimNode(const AnimNode &) = delete;
    AnimNode &AnimNode::operator=(AnimNode &) = delete;

public:
    virtual void Initialize_AnyThread(const AnimationInitializationContext &InContext) = 0;
    virtual void Update_AnyThread(const AnimationUpdateContext &InContext) = 0;
    virtual void Evaluate_AnyThread(PoseContext &Output) = 0;
    virtual void NodeDebug() = 0;

    // the special serialize and deserialize method for AnimNodes
    virtual void Serialize(rbc::ArchiveWrite &w) = 0;
    virtual void Deserialize(rbc::ArchiveRead &r) = 0;

    IndexType NodeID = INVALID_INDEX;
};

struct PoseLink : RCBase {
public:
    IndexType LinkedNodeID = INVALID_INDEX;// Serialized Link ID from Graph
protected:
    AnimNode *LinkedNode = nullptr;

public:
    void AttemptRelink(const AnimationBaseContext &InContext);
    AnimNode *GetLinkedNode();
    void Initialize(const AnimationInitializationContext &InContext);
    void Update(const AnimationUpdateContext &InContext);
    void Evaluate(PoseContext &Output);
};

}// namespace rbc

RBC_RTTI(rbc::AnimNode)
RBC_RTTI(rbc::PoseLink)