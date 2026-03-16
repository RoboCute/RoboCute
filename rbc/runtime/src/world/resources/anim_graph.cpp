#include "rbc_world/resources/anim_graph.h"
#include "rbc_world/type_register.h"
#include "rbc_anim/graph/AnimNode_SequencePlayer.h"
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {

void AnimGraphResource::serialize_meta(world::ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    // AnimGraph itself contains nodes that may reference other resources
    // The graph serialization handles the node data including resource references
}

void AnimGraphResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    // Dependencies will be resolved during _async_load when graph is deserialized
}

rbc::coroutine AnimGraphResource::_async_load() {
    std::shared_lock lck{_async_mtx};
    auto path = this->path();
    if (path.empty()) { co_return; }
    
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) { co_return; }

    luisa::BinaryBlob blob = file_stream.read(file_stream.length());
    BinDeSerializer deser{blob};
    deser._load(graph, "graph");

    // Collect and wait for all AnimSequenceResource dependencies from SequencePlayer nodes
    luisa::vector<RC<AnimSequenceResource>> anim_seq_deps;
    for (auto &node : graph.nodes) {
        if (auto seq_player = dynamic_cast<AnimNode_SequencePlayer *>(node.get())) {
            if (seq_player->anim_seq_resource) {
                anim_seq_deps.push_back(seq_player->anim_seq_resource);
            }
        }
    }

    // Wait for all animation sequence dependencies to load
    for (auto &dep : anim_seq_deps) {
        co_await dep->await_loading();
    }

    co_return;
}

bool AnimGraphResource::unsafe_save_to_path() const {
    //std::shared_lock lck{_async_mtx};
    //BinSerializer ser;
    //ser._store(graph, "graph");

    //auto path = this->path();
    //BinaryFileWriter writer{luisa::to_string(path)};
    //if (!writer._file) [[unlikely]] {
    //    return false;
    //}
    //LUISA_INFO("AnimGraph Writing to {}", path.string());
    //auto bytes = ser.write_to();
    //writer.write(bytes);
    return true;
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(AnimGraphResource)

}// namespace rbc::world
