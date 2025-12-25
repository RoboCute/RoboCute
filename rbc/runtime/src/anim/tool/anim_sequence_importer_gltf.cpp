#include "rbc_anim/tool/anim_sequence_importer_gltf.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "rbc_anim/tool/gltf2ozz.h"

namespace rbc {

bool GltfAnimSequenceImporter::import(AnimSequenceResource *resource, luisa::filesystem::path const &path) {
    LUISA_ASSERT(ref_skel.get());// RefSkeleton Should be valid
    GltfOzzImporter impl;
    ozz::animation::offline::OzzImporter &importer = impl;
    // TODO: static dependencies for SkeletonResource
}

}// namespace rbc