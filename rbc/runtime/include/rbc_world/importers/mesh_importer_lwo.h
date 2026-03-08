#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief LWO (LightWave Object) file importer for MeshResource
 * Supports LWO2 format (LightWave 6.0+)
 */
struct RBC_RUNTIME_API LwoMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".lwo"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
