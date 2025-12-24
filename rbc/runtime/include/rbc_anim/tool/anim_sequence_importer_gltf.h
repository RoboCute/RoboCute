#pragma once
#include "rbc_world/resource_importer.h"
#include "rbc_anim/resource/anim_sequence_resource.h"

namespace rbc {

struct RBC_RUNTIME_API GltfAnimSequenceImporter final : IAnimSequenceImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    [[nodiscard]] bool import(AnimSequenceResource *resource, luisa::filesystem::path const &path) override;
};

}// namespace rbc