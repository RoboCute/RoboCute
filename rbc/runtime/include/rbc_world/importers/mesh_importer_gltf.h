#pragma once
#include <rbc_world/resource_importer.h>
#include <rbc_world/util/gltf.h>

namespace rbc::world {

/**
 * @brief GLTF/GLB file importer for MeshResource
 */
struct RBC_RUNTIME_API GltfMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".gltf"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
    static bool import_from_data(MeshResource *resource, GltfImportData &import_data);
};

/**
 * @brief GLB (binary GLTF) file importer for MeshResource
 */
struct RBC_RUNTIME_API GlbMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".glb"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
