#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief PLY (Polygon File Format) file importer for MeshResource
 * Supports both ASCII and binary (little/big endian) formats
 */
struct PlyMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".ply"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
