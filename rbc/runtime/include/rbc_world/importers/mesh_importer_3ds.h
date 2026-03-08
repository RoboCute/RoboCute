#pragma once
#include <rbc_world/resource_importer.h>

namespace rbc::world {

/**
 * @brief 3DS (3D Studio) file importer for MeshResource
 */
struct RBC_RUNTIME_API ThreeDSMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".3ds"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;
};

}// namespace rbc::world
