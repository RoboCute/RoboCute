#include "rbc_anim/tool/anim_sequence_importer_gltf.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "rbc_anim/tool/gltf2ozz.h"
#include "ozz/animation/offline/animation_builder.h"
#include "rbc_core/memory.h"

namespace rbc {

bool GltfAnimSequenceImporter::import(AnimSequenceResource *resource, luisa::filesystem::path const &path) {
    LUISA_ASSERT(ref_skel.get());// RefSkeleton Should be valid
    GltfOzzImporter impl;
    ozz::animation::offline::OzzImporter &importer = impl;
    // TODO: static dependencies for SkeletonResource

    auto *skel = ref_skel.get();

    auto *raw_anim = RBCNew<RawAnimationAsset>();

    auto anim_names = importer.GetAnimationNames();
    if (!(anim_names.size() > 0)) {
        LUISA_ERROR("No Animation Found in GLTF File: {}", path.string());
        return false;
    } else {
        LUISA_INFO("{} anims found in {}", anim_names.size(), path.string());
    }
    if (chosen_anim_name.size() == 0) {
        // no specific choose, load first
        chosen_anim_name = anim_names[0].c_str();
    }

    importer.Import(
        chosen_anim_name.c_str(),
        skel->ref_skel().GetRawSkeleton(),
        sampling_rate, raw_anim);

    // Cook
    ozz::animation::offline::AnimationBuilder builder;
    ozz::unique_ptr<ozz::animation::Animation> animation = builder(*raw_anim);
    if (!animation) {
        LUISA_ERROR("Failed to Cook Animation");
        return false;
    }
    seq_ref(resource) = std::move(*animation);
    skel_ref(resource) = ref_skel;

    RBCDelete(raw_anim);
}

}// namespace rbc