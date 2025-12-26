#pragma once

#include "rbc_world/resources/skin.h"
#include "rbc_world/util/gltf.h"

namespace rbc {

struct RBC_RUNTIME_API GltfSkinImporter final : ISkinImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    [[nodiscard]] bool import(SkinResource *resource, luisa::filesystem::path const &path) override;
    static bool import_from_data(SkinResource *resource, GltfImportData &import_data);
};

}// namespace rbc