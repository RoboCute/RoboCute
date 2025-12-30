#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <cstring>
#include <algorithm>
#include "rbc_world/util/gltf_scene_loader.h"
#include "rbc_world/util/gltf.h"

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool GltfMeshImporter::import(MeshResource *resource, luisa::filesystem::path const &path) {
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    tinygltf::Model model;
    if (!load_gltf_model(model, path, false)) {
        return false;
    }
    GltfImportData import_data = process_gltf_model(model);
    return import_from_data(resource, import_data);
}

bool GltfMeshImporter::import_from_data(MeshResource *resource, GltfImportData &import_data) {
    MeshBuilder &mesh_builder = import_data.mesh_builder;

    if (mesh_builder.position.empty()) {
        return false;
    }

    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty({}, std::move(submesh_offsets), 0, mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);

    // skinning
    if (!import_data.all_skin_weights.empty()) {
        size_t weight_size = import_data.max_weight_count;

        auto property = resource->add_property(
            "skin_attrib",
            weight_size * resource->vertex_count() * sizeof(SkinAttrib));

        auto skin_weights = luisa::span{
            (SkinAttrib *)property.second.data(),
            property.second.size()};

        // 从导出开始已经同构，直接复制即可
        std::memcpy(skin_weights.data(), import_data.all_skin_weights.data(), skin_weights.size_bytes());
    }
    return true;
}

bool GlbMeshImporter::import(MeshResource *resource, luisa::filesystem::path const &path) {
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }
    tinygltf::Model model;
    if (!load_gltf_model(model, path, true)) {
        return false;
    }

    GltfImportData import_data = process_gltf_model(model);
    MeshBuilder &mesh_builder = import_data.mesh_builder;

    if (mesh_builder.position.empty()) {
        return false;
    }

    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty({}, std::move(submesh_offsets), 0, mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);

    // skinning
    if (!import_data.all_skin_weights.empty()) {
        size_t weight_size = import_data.max_weight_count;
        auto property = resource->add_property(
            "skin_attrib",
            weight_size * resource->vertex_count() * sizeof(SkinAttrib));

        auto skin_weights = luisa::span{
            (SkinAttrib *)property.second.data(),
            property.second.size()};
        // 从导出开始已经同构，直接复制即可
        std::memcpy(skin_weights.data(), import_data.all_skin_weights.data(), skin_weights.size_bytes());
    }
    return true;
}

}// namespace rbc::world
