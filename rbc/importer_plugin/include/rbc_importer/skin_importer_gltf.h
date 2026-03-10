#pragma once

#include "rbc_world/resources/skin.h"
#include "rbc_importer/gltf.h"

namespace rbc {

struct GltfSkinImporter final : world::ISkinImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    bool import(world::Resource *res, luisa::filesystem::path const &path) override;
};

}// namespace rbc
