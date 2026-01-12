#pragma once

#include "rbc_world/resources/skin.h"
#include "rbc_world/util/gltf.h"

namespace rbc {

struct RBC_RUNTIME_API GltfSkinImporter final : ISkinImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    bool import(world::Resource *res, luisa::filesystem::path const &path) override;
};

}// namespace rbc