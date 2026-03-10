#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief FBX file importer for MeshResource
 */
struct FbxMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".fbx"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
