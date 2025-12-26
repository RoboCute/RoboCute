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
#include "rbc_world/importers/gltf_scene_loader.h"
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

    // Setup device mesh - access protected members from member function
    auto &device_mesh = device_mesh_ref(resource);
    if (!device_mesh)
        device_mesh = RC<DeviceMesh>(new DeviceMesh());

    vertex_count_ref(resource) = mesh_builder.vertex_count();
    triangle_count_ref(resource) = 0;
    for (auto &i : mesh_builder.triangle_indices) {
        triangle_count_ref(resource) += i.size() / 3;
    }
    uv_count_ref(resource) = mesh_builder.uv_count();
    set_contained_normal(resource, mesh_builder.contained_normal());
    set_contained_tangent(resource, mesh_builder.contained_tangent());
    mesh_builder.write_to(device_mesh->host_data_ref(), submesh_offsets_ref(resource));

    // Write skinning weights
    if (import_data.max_weight_count > 0 && !import_data.all_skin_weights.empty()) {
        // skin_attrib_count_ref(resource) = import_data.max_weight_count;
        size_t total_weight_size = vertex_count_ref(resource) * import_data.max_weight_count;

        auto property = resource->add_property(
            "skin_attrib",
            total_weight_size * sizeof(SkinAttrib));

        auto skin_weights = luisa::span{
            (SkinAttrib *)property.second.data(),
            property.second.size()};

        std::memset(skin_weights.data(), 0, skin_weights.size_bytes());

        // Copy skin weights (already in the correct format)
        size_t copy_size = std::min(import_data.all_skin_weights.size(), total_weight_size);
        std::memcpy(skin_weights.data(), import_data.all_skin_weights.data(), copy_size * sizeof(SkinAttrib));
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

    // Setup device mesh - access protected members from member function
    auto &device_mesh = device_mesh_ref(resource);
    if (!device_mesh)
        device_mesh = RC<DeviceMesh>(new DeviceMesh());

    vertex_count_ref(resource) = mesh_builder.vertex_count();
    triangle_count_ref(resource) = 0;
    for (auto &i : mesh_builder.triangle_indices) {
        triangle_count_ref(resource) += i.size() / 3;
    }
    uv_count_ref(resource) = mesh_builder.uv_count();
    set_contained_normal(resource, mesh_builder.contained_normal());
    set_contained_tangent(resource, mesh_builder.contained_tangent());

    mesh_builder.write_to(device_mesh->host_data_ref(), submesh_offsets_ref(resource));

    // Write skinning weights

    if (import_data.max_weight_count > 0 && !import_data.all_skin_weights.empty()) {
        size_t total_weight_size = vertex_count_ref(resource) * import_data.max_weight_count;
        auto property = resource->add_property("skin_attrib", total_weight_size * sizeof(SkinAttrib));
        auto skin_weights = property.second;
        std::memset(skin_weights.data(), 0, skin_weights.size_bytes());

        // Copy skin weights (already in the correct format)
        size_t copy_size = std::min(import_data.all_skin_weights.size(), total_weight_size);
        std::memcpy(skin_weights.data(), import_data.all_skin_weights.data(), copy_size * sizeof(SkinAttrib));
    }
    return true;
}

}// namespace rbc::world
