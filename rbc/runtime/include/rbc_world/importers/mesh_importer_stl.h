#pragma once
#include <rbc_world/resource_importer.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/dll_export.h>
#include <luisa/core/stl/vector.h>
#include <cstddef>

namespace rbc::world {

/**
 * @brief STL (Stereolithography) file importer for MeshResource
 * Supports both binary and ASCII STL formats
 */
struct RBC_RUNTIME_API StlMeshImporter final : IMeshImporter {
    [[nodiscard]] luisa::string_view extension() const override { return ".stl"; }

    bool import(Resource *resource_base, luisa::filesystem::path const &path) override;

private:
    /**
     * @brief Check if the STL file is binary format
     * @param data The file data to check
     * @return true if binary format, false if ASCII
     */
    [[nodiscard]] bool _is_binary_format(luisa::span<const std::byte> data) const;

    /**
     * @brief Import binary STL file
     * @param data The file data to read from
     * @param mesh_builder The mesh builder to populate
     * @return true on success, false on failure
     */
    [[nodiscard]] bool _import_binary(
        luisa::span<const std::byte> data,
        MeshBuilder &mesh_builder) const;

    /**
     * @brief Import ASCII STL file
     * @param data The file data to read from
     * @param mesh_builder The mesh builder to populate
     * @return true on success, false on failure
     */
    [[nodiscard]] bool _import_ascii(
        luisa::span<const std::byte> data,
        MeshBuilder &mesh_builder) const;

    /**
     * @brief Compute face normal from triangle vertices
     * @param v0 First vertex
     * @param v1 Second vertex
     * @param v2 Third vertex
     * @return Normalized face normal
     */
    [[nodiscard]] float3 _compute_normal(
        const float3 &v0,
        const float3 &v1,
        const float3 &v2) const;
};

}// namespace rbc::world
