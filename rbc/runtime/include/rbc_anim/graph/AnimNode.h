#pragma once
#include "rbc_anim/types.h"
#include "rbc_anim/graph/AnimNodeContext.h"

namespace rbc {

struct AnimNode {
public:
    virtual void Initialize_AnyThread(const AnimationInitializationContext &InContext) = 0;
    virtual void Update_AnyThread(const AnimationUpdateContext &InContext) = 0;
    virtual void Evaluate_AnyThread(PoseContext &Output) = 0;
    virtual void NodeDebug() = 0;

    IndexType NodeID = INVALID_INDEX;
};

struct PoseLink {
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