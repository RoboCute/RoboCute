#pragma once

#include "rbc_anim/resource/skin_resource.h"

namespace rbc {

struct RBC_RUNTIME_API GltfSkinImporter final : ISkinImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    [[nodiscard]] bool import(SkinResource *resource, luisa::filesystem::path const &path) override;
};

}// namespace rbc