#include "rbc_core/memory.h"
#include "rbc_world/importers/skel_importer_gltf.h"
#include "rbc_world/importers/gltf2ozz.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "rbc_anim/types.h"
#include <tracy_wrapper.h>

namespace rbc {
bool GltfSkeletonImporter::import(world::Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<SkeletonResource *>(resource_base);
    GltfOzzImporter impl;
    ozz::animation::offline::OzzImporter &importer = impl;
    ozz::animation::offline::OzzImporter::NodeType types = {};
    types.skeleton = true;

    if (!importer.Load(path.string().c_str())) {
        LUISA_ERROR("Failed to load gltf file {} for Skeleton", path.string());
        resource = nullptr;
        return false;
    }
    auto raw_skel = RBCNew<RawSkeletonAsset>();

    importer.Import(raw_skel, types);

    // Cook: Raw Skeleton Asset -> Runtime Skeleton Asset
    {
        RBCZoneScopedN("Cook Skeleton Asset");
        using namespace ozz::animation::offline;
        SkeletonBuilder builder;
        ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(*raw_skel);
        if (!skeleton) {
            LUISA_ERROR("Failed to build skeleton resource");
        }
        ref_skel(resource) = std::move(*skeleton);
    }

    RBCDelete(raw_skel);
    return true;
}

}// namespace rbc