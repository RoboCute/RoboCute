#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief PLY file importer for GaussianSplatResource
 * Imports 3D Gaussian Splatting data from PLY files
 */
struct PlyGaussianSplatImporter final : IResourceImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".ply"; }
    [[nodiscard]] MD5 resource_type() const override;

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
