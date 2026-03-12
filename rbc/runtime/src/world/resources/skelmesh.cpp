#include "rbc_world/resources/skelmesh.h"
#include "rbc_world/type_register.h"
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {

void SkelMeshResource::serialize_meta(world::ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    // Serialize resource dependencies
    if (ref_skin) {
        ser.ar.value(ref_skin->guid(), "ref_skin");
    }
    if (ref_skeleton) {
        ser.ar.value(ref_skeleton->guid(), "ref_skeleton");
    }
    if (ref_anim_graph) {
        ser.ar.value(ref_anim_graph->guid(), "ref_anim_graph");
    }
}

void SkelMeshResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    // Deserialize resource dependencies
    vstd::Guid ref_skin_guid;
    if (ser.ar.value(ref_skin_guid, "ref_skin")) {
        auto res = get_resource(ref_skin_guid, true);
        if (res && res->is_type_of(TypeInfo::get<SkinResource>())) {
            ref_skin = res;
        } else {
            ref_skin = nullptr;
        }
    }
    
    vstd::Guid ref_skeleton_guid;
    if (ser.ar.value(ref_skeleton_guid, "ref_skeleton")) {
        auto res = get_resource(ref_skeleton_guid, true);
        if (res && res->is_type_of(TypeInfo::get<SkeletonResource>())) {
            ref_skeleton = res;
        } else {
            ref_skeleton = nullptr;
        }
    }
    
    vstd::Guid ref_anim_graph_guid;
    if (ser.ar.value(ref_anim_graph_guid, "ref_anim_graph")) {
        auto res = get_resource(ref_anim_graph_guid, true);
        if (res && res->is_type_of(TypeInfo::get<AnimGraphResource>())) {
            ref_anim_graph = res;
        } else {
            ref_anim_graph = nullptr;
        }
    }
}

rbc::coroutine SkelMeshResource::_async_load() {
    // Wait for all dependencies to load
    if (ref_skin) {
        co_await ref_skin->await_loading();
    }
    if (ref_skeleton) {
        co_await ref_skeleton->await_loading();
    }
    if (ref_anim_graph) {
        co_await ref_anim_graph->await_loading();
    }
    
    // SkelMeshResource itself doesn't have additional binary data to load
    // It's a composite resource that references other resources
    co_return;
}

bool SkelMeshResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    // SkelMeshResource is a meta-only resource, no binary data to save
    // The actual data is stored in its dependencies (skin, skeleton, anim_graph)
    return true;
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkelMeshResource)

}// namespace rbc
