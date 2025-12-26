#include "rbc_world/importers/anim_sequence_importer_gltf.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "rbc_world/importers/gltf2ozz.h"
#include "ozz/animation/offline/animation_builder.h"
#include "rbc_core/memory.h"

namespace rbc::world {

bool GltfAnimSequenceImporter::import(AnimSequenceResource *resource, luisa::filesystem::path const &path) {
    LUISA_ASSERT(ref_skel.get());// RefSkeleton Should be valid
    GltfOzzImporter impl;
    ozz::animation::offline::OzzImporter &importer = impl;

    auto *skel = ref_skel.get();
    if (!skel) {
        LUISA_ERROR("Import AnimSequence Should Depend on a Valid SkeletonResource");
    }
    if (!importer.Load(path.string().c_str())) {
        LUISA_ERROR("Failed to load gltf {} for AnimSeq", path.string());
    }

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

    return true;
}

}// namespace rbc::world