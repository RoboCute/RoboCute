#pragma once

namespace rbc {
class AnimNode;
}// namespace rbc

namespace rbc {

// 一个会存储下来的AnimGraph资产
struct RBC_RUNTIME_API AnimGraph {
    luisa::vector<RC<AnimNode>> nodes;// record for all nodes

public:
    rbc::AnimNode *GetRootNode();
};

template<>
struct RBC_RUNTIME_API Serialize<AnimGraph> {
    static void read(rbc::ArchiveRead &r, AnimGraph &v);
    static void write(rbc::ArchiveWrite &w, const AnimGraph &v);
};

}// namespace rbc