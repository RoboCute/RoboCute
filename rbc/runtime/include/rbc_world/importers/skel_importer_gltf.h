#pragma once

#include "rbc_world/resources/skeleton.h"

namespace rbc {

struct RBC_RUNTIME_API GltfSkeletonImporter final : ISkeletonImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }
    [[nodiscard]] bool import(SkeletonResource *resource, luisa::filesystem::path const &path) override;
};

}// namespace rbc