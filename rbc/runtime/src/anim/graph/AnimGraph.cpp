#include "rbc_anim/graph/AnimGraph.h"

namespace rbc {
AnimNode *AnimGraph::GetRootNode() {
    LUISA_ASSERT(nodes.size() > 0);
    return nodes[0].get();
}
void Serialize<AnimGraph>::write(ArchiveWrite &w, const AnimGraph &v) {}
void Serialize<AnimGraph>::read(ArchiveRead &r, AnimGraph &v) {}

}// namespace rbc
