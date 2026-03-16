#include <rbc_world/resources/mesh.h>
#include <rbc_importer/mesh_importer_gltf.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <cstring>
// #include <algorithm>  // Unused include
#include "rbc_importer/gltf_scene_loader.h"
#include "rbc_importer/gltf.h"

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;
bool GltfMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
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

    // Size check: ensure all vertex attributes have the same size as positions
    size_t position_size = mesh_builder.position.size();

    // Resize normals to match position size if needed
    if (!mesh_builder.normal.empty() && mesh_builder.normal.size() != position_size) {
        mesh_builder.normal.resize(position_size, float3(0.0f, 0.0f, 0.0f));
    }

    // Resize tangents to match position size if needed
    if (!mesh_builder.tangent.empty() && mesh_builder.tangent.size() != position_size) {
        mesh_builder.tangent.resize(position_size, float4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Resize UVs to match position size if needed
    for (auto &uv_set : mesh_builder.uvs) {
        if (!uv_set.empty() && uv_set.size() != position_size) {
            uv_set.resize(position_size, float2(0.0f, 0.0f));
        }
    }

    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(std::move(submesh_offsets), mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);

    // skinning
    if (import_data.max_weight_count > 0) {
        size_t weight_size = import_data.max_weight_count;
        LUISA_ASSERT(weight_size == 4, "Skinning Weight Size is not 4");

        auto property = resource->add_property(
            "joint_index",
            weight_size * resource->vertex_count() * sizeof(uint16_t));

        auto joint_index = luisa::span{
            (uint16_t *)property.second.data(),
            property.second.size()};

        std::memcpy(joint_index.data(), import_data.all_joint_index.data(), joint_index.size_bytes());

        auto w_property = resource->add_property(
            "joint_weight",
            weight_size * resource->vertex_count() * sizeof(uint16_t));

        auto joint_weight = luisa::span{
            reinterpret_cast<float *>(w_property.second.data()),
            w_property.second.size() / sizeof(float)};

        std::memcpy(joint_weight.data(), import_data.all_joint_weight.data(), joint_weight.size_bytes());
    }
    return true;
}
bool GlbMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
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

    // Size check: ensure all vertex attributes have the same size as positions
    size_t position_size = mesh_builder.position.size();

    // Resize normals to match position size if needed
    if (!mesh_builder.normal.empty() && mesh_builder.normal.size() != position_size) {
        mesh_builder.normal.resize(position_size, float3(0.0f, 0.0f, 0.0f));
    }

    // Resize tangents to match position size if needed
    if (!mesh_builder.tangent.empty() && mesh_builder.tangent.size() != position_size) {
        mesh_builder.tangent.resize(position_size, float4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Resize UVs to match position size if needed
    for (auto &uv_set : mesh_builder.uvs) {
        if (!uv_set.empty() && uv_set.size() != position_size) {
            uv_set.resize(position_size, float2(0.0f, 0.0f));
        }
    }

    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(std::move(submesh_offsets), mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);

    // skinning
    if (import_data.max_weight_count > 0) {
        size_t weight_size = import_data.max_weight_count;

        auto property = resource->add_property(
            "joint_index",
            weight_size * resource->vertex_count() * sizeof(uint16_t));

        auto joint_index = luisa::span{
            (uint16_t *)property.second.data(),
            property.second.size()};

        std::memcpy(joint_index.data(), import_data.all_joint_index.data(), joint_index.size_bytes());

        auto w_property = resource->add_property(
            "joint_weight",
            weight_size * resource->vertex_count() * sizeof(uint16_t));

        auto joint_weight = luisa::span{
            (float *)w_property.second.data(),
            w_property.second.size()};

        std::memcpy(joint_weight.data(), import_data.all_joint_weight.data(), joint_weight.size_bytes());
    }
    return true;
}

}// namespace rbc::world
