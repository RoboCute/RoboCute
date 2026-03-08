#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief Alembic (.abc) file importer for MeshResource
 */
struct RBC_RUNTIME_API AbcMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".abc"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
