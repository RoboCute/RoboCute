#include "rbc_importer/anim_sequence_importer_gltf.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "rbc_importer/gltf2ozz.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/base/memory/allocator.h"

namespace rbc::world {

bool GltfAnimSequenceImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<AnimSequenceResource *>(resource_base);

    // Get skeleton from resource's ref_skel (must be set before import)
    auto skel = resource->ref_skel;

    LUISA_ASSERT(skel.get());// RefSkeleton Should be valid
    GltfOzzImporter impl;
    ozz::animation::offline::OzzImporter &importer = impl;

    auto *skel_ptr = skel.get();
    if (!skel_ptr) {
        LUISA_ERROR("Import AnimSequence Should Depend on a Valid SkeletonResource");
    }
    if (!importer.Load(path.string().c_str())) {
        LUISA_ERROR("Failed to load gltf {} for AnimSeq", path.string());
    }

    auto *raw_anim = ozz::New<RawAnimationAsset>();

    auto anim_names = importer.GetAnimationNames();

    if (!(anim_names.size() > 0)) {
        LUISA_ERROR("No Animation Found in GLTF File: {}", path.string());
        return false;
    } else {
        LUISA_INFO("{} anims found in {}", anim_names.size(), path.string());
    }

    // Get animation name from resource's anim_name (set via meta or directly)
    auto anim_name = resource->anim_name;
    if (anim_name.empty()) {
        // no specific choose, load first
        anim_name = anim_names[0].c_str();
    }

    // Get sampling rate from resource's sampling_rate (set via meta or directly)
    auto rate = resource->sampling_rate;
    if (rate <= 0.0f) {
        rate = 30.0f;// default sampling rate
    }

    importer.Import(
        anim_name.c_str(),
        skel_ptr->ref_skel().GetRawSkeleton(),
        rate, raw_anim);

    // Cook
    ozz::animation::offline::AnimationBuilder builder;
    ozz::unique_ptr<ozz::animation::Animation> animation = builder(*raw_anim);
    if (!animation) {
        LUISA_ERROR("Failed to Cook Animation");
        return false;
    }
    resource->ref_seq() = std::move(*animation);
    resource->ref_skel = skel;

    ozz::Delete(raw_anim);

    return true;
}

}// namespace rbc::world