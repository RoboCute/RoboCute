#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief OBJ file importer for MeshResource
 */
struct RBC_RUNTIME_API ObjMeshImporter final : IMeshImporter {
    luisa::string_view extension() const override { return ".obj"; }
    
    bool import(MeshResource *resource, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world

