#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief GLTF/GLB file importer for MeshResource
 */
struct RBC_RUNTIME_API GltfMeshImporter final : IMeshImporter {
    luisa::string_view extension() const override { return ".gltf"; }
    
    bool import(MeshResource *resource, luisa::filesystem::path const &path) override;
};

/**
 * @brief GLB (binary GLTF) file importer for MeshResource
 */
struct RBC_RUNTIME_API GlbMeshImporter final : IMeshImporter {
    luisa::string_view extension() const override { return ".glb"; }
    
    bool import(MeshResource *resource, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world

