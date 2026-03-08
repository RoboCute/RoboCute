#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief OFF file importer for MeshResource
 * OFF (Object File Format) is a simple geometry format that stores
 * vertices and polygon faces. It does not support normals, UVs, or materials.
 */
struct RBC_RUNTIME_API OffMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".off"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
