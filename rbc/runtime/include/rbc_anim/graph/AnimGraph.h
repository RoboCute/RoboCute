#pragma once
#include "rbc_world/base_object.h"
#include "rbc_anim/graph/AnimNode.h"

namespace rbc {

// 一个会存储下来的AnimGraph资产
struct RBC_RUNTIME_API AnimGraph : RCBase {
    luisa::vector<RC<AnimNode>> nodes;// record for all nodes

public:
    rbc::AnimNode *GetRootNode();
};

template<>
struct RBC_RUNTIME_API Serialize<AnimGraph> {
    static void write(rbc::ArchiveWrite &w, const AnimGraph &v);
    static void read(rbc::ArchiveRead &r, AnimGraph &v);
};

}// namespace rbc

RBC_RTTI(rbc::AnimGraph)