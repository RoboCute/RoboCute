#include "rbc_anim/graph/AnimGraph.h"

namespace rbc {
AnimNode *AnimGraph::GetRootNode() {
    LUISA_ASSERT(nodes.size() > 0);
    return nodes[0].get();
}
bool Serialize<AnimGraph>::write(ArchiveWrite &w, const AnimGraph &v) {
    w.start_array();
    for (auto &node : v.nodes) {
        node->Serialize(w);
    }
    w.end_array("data");
    return true;
}
bool Serialize<AnimGraph>::read(ArchiveRead &r, AnimGraph &v) {
    size_t size;
    r.start_array(size, "data");
    v.nodes.resize_uninitialized(size);
    for (auto i = 0; i < size; i++) {
        v.nodes[i]->Deserialize(r);
    }
    r.end_scope();
    return true;
}

}// namespace rbc
