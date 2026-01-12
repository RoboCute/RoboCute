#pragma once
#include "rbc_world/resource_importer.h"
#include "rbc_world/resources/anim_sequence.h"
#include "rbc_world/resources/skeleton.h"

namespace rbc::world {

struct RBC_RUNTIME_API GltfAnimSequenceImporter final : IAnimSequenceImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    [[nodiscard]] bool import(Resource *resource, luisa::filesystem::path const &path) override;
    // dependencies
    RC<SkeletonResource> ref_skel;
    luisa::string chosen_anim_name;
    float sampling_rate = 30.0f;
};

}// namespace rbc::world