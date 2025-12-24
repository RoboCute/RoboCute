#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_obj.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <tiny_obj_loader.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool ObjMeshImporter::import(MeshResource *resource, luisa::filesystem::path const &path) {
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }
    
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return false;
    
    tinyobj::ObjReaderConfig obj_reader_config;
    obj_reader_config.triangulate = true;
    obj_reader_config.vertex_color = false;
    tinyobj::ObjReader obj_reader;
    std::string str;
    str.resize(file_stream.length());
    file_stream.read(
        {(std::byte *)(str.data()),
         str.size()});
    if (!obj_reader.ParseFromString(str, "", obj_reader_config)) {
        luisa::string_view error_message = "unknown error.";
        if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
        return false;
    }
    if (auto &&e = obj_reader.Warning(); !e.empty()) {
        LUISA_WARNING_WITH_LOCATION("{}", e);
    }
    auto &attri = obj_reader.GetAttrib();
    MeshBuilder mesh_builder;
    {
        auto &p = attri.vertices;
        mesh_builder.position.reserve(p.size() / 3);
        for (uint i = 0u; i < p.size(); i += 3u) {
            mesh_builder.position.push_back(make_float3(
                p[i + 0u], p[i + 1u], p[i + 2u]));
        }
    }

    {
        auto &p = attri.normals;
        if (!p.empty()) {
            mesh_builder.normal.reserve(p.size() / 3);
            for (uint i = 0u; i < p.size(); i += 3u) {
                mesh_builder.normal.push_back(make_float3(
                    p[i + 0u], p[i + 1u], p[i + 2u]));
            }
        }
    }
    {
        auto &p = attri.texcoords;
        if (!p.empty()) {
            auto &uvs = mesh_builder.uvs.emplace_back();
            uvs.reserve(p.size() / 2);
            for (uint i = 0u; i < p.size(); i += 2u) {
                uvs.push_back(make_float2(
                    p[i + 0u], p[i + 1u]));
            }
        }
    }
    {
        auto &shapes = obj_reader.GetShapes();
        for (auto &i : shapes) {
            if (i.mesh.indices.empty()) continue;
            auto &ind = mesh_builder.triangle_indices.emplace_back();
            ind.reserve(i.mesh.indices.size());
            for (auto &idx : i.mesh.indices) {
                ind.emplace_back(idx.vertex_index);
            }
        }
    }
    if (mesh_builder.uv_count() > 0 && mesh_builder.normal.size() > 0) {
        mesh_builder.tangent.push_back_uninitialized(mesh_builder.vertex_count());
        if (mesh_builder.triangle_indices.size() > 1) {
            luisa::vector<Triangle> triangles;
            uint64_t size = 0;
            for (auto &i : mesh_builder.triangle_indices) {
                size += i.size() / 3;
            }
            triangles.reserve(size);
            for (auto &i : mesh_builder.triangle_indices) {
                vstd::push_back_all(triangles, luisa::span{(Triangle *)i.data(), i.size() / 3});
            }
            calculate_tangent(mesh_builder.position, mesh_builder.uvs[0], mesh_builder.tangent, triangles, 1);
        } else {
            calculate_tangent(
                mesh_builder.position,
                mesh_builder.uvs[0],
                mesh_builder.tangent,
                luisa::span{
                    (Triangle const *)(mesh_builder.triangle_indices[0].data()),
                    mesh_builder.triangle_indices[0].size() / 3},
                1);
        }
    }
    
    auto &device_mesh = IMeshImporter::device_mesh_ref(resource);
    if (!device_mesh)
        device_mesh = RC<DeviceMesh>(new DeviceMesh());
    
    IMeshImporter::vertex_count_ref(resource) = mesh_builder.vertex_count();
    IMeshImporter::triangle_count_ref(resource) = 0;
    for (auto &i : mesh_builder.triangle_indices) {
        IMeshImporter::triangle_count_ref(resource) += i.size() / 3;
    }
    IMeshImporter::uv_count_ref(resource) = mesh_builder.uv_count();
    IMeshImporter::set_contained_normal(resource, mesh_builder.contained_normal());
    IMeshImporter::set_contained_tangent(resource, mesh_builder.contained_tangent());
    mesh_builder.write_to(device_mesh->host_data_ref(), IMeshImporter::submesh_offsets_ref(resource));
    
    // skinning
    IMeshImporter::skinning_weight_count_ref(resource) = attri.skin_weights.size() / mesh_builder.position.size();
    size_t weight_size = 0;
    for (auto &i : attri.skin_weights) {
        weight_size = std::max<size_t>(weight_size, i.weightValues.size());
    }
    auto start_index = device_mesh->host_data_ref().size();
    device_mesh->host_data_ref().push_back_uninitialized(weight_size * IMeshImporter::vertex_count_ref(resource) * sizeof(SkinWeight));
    luisa::span<SkinWeight> skin_weights{
        (SkinWeight *)device_mesh->host_data_ref().data() + start_index,
        weight_size * IMeshImporter::vertex_count_ref(resource)};
    std::memset(skin_weights.data(), 0, skin_weights.size_bytes());
    for (auto &i : attri.skin_weights) {
        auto skin_ptr = &skin_weights[i.vertex_id * weight_size];
        for (auto &j : i.weightValues) {
            skin_ptr->weight = j.weight;
            skin_ptr->joint_id = j.joint_id;
            ++skin_ptr;
        }
    }
    // vertex_color
    IMeshImporter::vertex_color_channels_ref(resource) = attri.colors.size() / mesh_builder.position.size();
    if (IMeshImporter::vertex_color_channels_ref(resource) > 0)
        vstd::push_back_all(
            device_mesh->host_data_ref(),
            luisa::span{
                (std::byte *)attri.colors.data(),
                attri.colors.size() * sizeof(tinyobj::real_t)});
    return true;
}

}// namespace rbc::world

