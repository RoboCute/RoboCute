#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_fbx.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/rtx/triangle.h>
#include <ofbx.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool FbxMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    // Read file data
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) {
        LUISA_WARNING("Failed to open FBX file: {}", path.string());
        return false;
    }

    luisa::vector<ofbx::u8> file_data;
    file_data.resize(file_stream.length());
    file_stream.read(luisa::span{reinterpret_cast<std::byte *>(file_data.data()), file_data.size()});

    // Load FBX scene
    ofbx::LoadFlags load_flags = ofbx::LoadFlags::NONE;
    ofbx::IScene *scene = ofbx::load(file_data.data(), file_data.size(), static_cast<ofbx::u16>(load_flags));
    if (!scene) {
        LUISA_WARNING("Failed to parse FBX file: {} - {}", path.string(), ofbx::getError());
        return false;
    }

    MeshBuilder mesh_builder;
    luisa::vector<ofbx::u32> tri_indices;
    luisa::vector<SkinAttrib> skin_attribs;
    size_t max_skin_weights_per_vertex = 0;

    // Process all meshes in the scene
    int mesh_count = scene->getMeshCount();
    for (int mesh_idx = 0; mesh_idx < mesh_count; ++mesh_idx) {
        const ofbx::Mesh *mesh = scene->getMesh(mesh_idx);
        if (!mesh) continue;

        const ofbx::Geometry *geom = mesh->getGeometry();
        if (!geom) continue;

        const ofbx::GeometryData &geom_data = geom->getGeometryData();
        if (!geom_data.hasVertices()) continue;

        // Get geometry data
        ofbx::Vec3Attributes positions = geom_data.getPositions();
        ofbx::Vec3Attributes normals = geom_data.getNormals();
        ofbx::Vec2Attributes uv_groups[4] = {geom_data.getUVs(0),
                                             geom_data.getUVs(1),
                                             geom_data.getUVs(2),
                                             geom_data.getUVs(3)};

        if (positions.count == 0) continue;

        // Track the starting vertex offset for this mesh
        size_t vertex_offset = mesh_builder.position.size();

        // Add positions
        for (int i = 0; i < positions.values_count; ++i) {
            ofbx::Vec3 pos = positions.values[i];
            mesh_builder.position.push_back(make_float3(static_cast<float>(pos.x),
                                                        static_cast<float>(pos.y),
                                                        static_cast<float>(pos.z)));
        }

        // Add normals if available, resize to match position size if needed
        if (normals.values && normals.values_count > 0) {
            for (int i = 0; i < normals.values_count; ++i) {
                ofbx::Vec3 n = normals.values[i];
                mesh_builder.normal.push_back(make_float3(static_cast<float>(n.x),
                                                          static_cast<float>(n.y),
                                                          static_cast<float>(n.z)));
            }
            if (mesh_builder.normal.size() < static_cast<size_t>(positions.values_count)) {
                mesh_builder.normal.resize(positions.values_count, make_float3(0.0f, 0.0f, 1.0f));
            }
        }

        // Add UVs if available, resize to match position size if needed
        if (mesh_builder.uvs.empty()) {
            mesh_builder.uvs.resize(vstd::array_count(uv_groups));
        }
        auto iter = mesh_builder.uvs.begin();
        for (auto uvs : uv_groups) {
            auto &uv_vec = *iter;
            ++iter;
            if (uvs.values && uvs.count > 0) {
                uv_vec.reserve(positions.values_count);
                int count = std::min(uvs.count, positions.values_count);
                for (int i = 0; i < count; ++i) {
                    ofbx::Vec2 uv = uvs.values[i];
                    uv_vec.emplace_back(static_cast<float>(uv.x),
                                        static_cast<float>(uv.y));
                }
            }
            // Resize to match position size if needed
            if (uv_vec.size() < static_cast<size_t>(positions.values_count)) {
                uv_vec.resize(positions.values_count, make_float2(0.0f, 0.0f));
            }
        }

        // Process partitions (submeshes with different materials)
        int partition_count = geom_data.getPartitionCount();
        for (int part_idx = 0; part_idx < partition_count; ++part_idx) {
            ofbx::GeometryPartition partition = geom_data.getPartition(part_idx);

            auto &indices = mesh_builder.triangle_indices.emplace_back();

            // Triangulate polygons
            for (int poly_idx = 0; poly_idx < partition.polygon_count; ++poly_idx) {
                const ofbx::GeometryPartition::Polygon &polygon = partition.polygons[poly_idx];
                tri_indices.resize(polygon.vertex_count);
                ofbx::u32 tri_count = ofbx::triangulate(geom_data, polygon,
                                                        reinterpret_cast<int *>(tri_indices.data()),
                                                        nullptr);
                tri_indices.resize(tri_count);
                // Add triangle indices with vertex offset
                // ofbx::triangulate returns indices into positions.indices array
                // (i.e., tri_indices values already include polygon.from_vertex)
                for (ofbx::u32 i = 0; i < tri_count; ++i) {
                    int global_idx = tri_indices[i];
                    // Get the actual vertex index from the positions indices array
                    int vertex_idx = positions.indices ? positions.indices[global_idx] : global_idx;
                    indices.emplace_back(static_cast<uint>(vertex_offset + vertex_idx));
                }
            }
        }

        // Process skinning if available
        const ofbx::Skin *skin = geom->getSkin();
        if (skin) {
            int cluster_count = skin->getClusterCount();
            luisa::vector<luisa::vector<SkinAttrib>> vertex_skin_attribs(positions.values_count);

            for (int cluster_idx = 0; cluster_idx < cluster_count; ++cluster_idx) {
                const ofbx::Cluster *cluster = skin->getCluster(cluster_idx);
                if (!cluster) continue;

                const int *indices = cluster->getIndices();
                const double *weights = cluster->getWeights();
                int idx_count = cluster->getIndicesCount();

                if (!indices || !weights || idx_count == 0) continue;

                // Get link (bone) ID
                uint16_t joint_id = static_cast<uint16_t>(cluster_idx);

                for (int i = 0; i < idx_count; ++i) {
                    int vertex_idx = indices[i];
                    float weight = static_cast<float>(weights[i]);

                    if (vertex_idx >= 0 && vertex_idx < static_cast<int>(vertex_skin_attribs.size())) {
                        SkinAttrib attrib;
                        attrib.joint_id = joint_id;
                        attrib.weight = weight;
                        vertex_skin_attribs[vertex_idx].push_back(attrib);
                    }
                }
            }

            // Find max weights per vertex and normalize
            size_t max_weights = 0;
            for (auto &attribs : vertex_skin_attribs) {
                max_weights = std::max(max_weights, attribs.size());
            }
            max_skin_weights_per_vertex = std::max(max_skin_weights_per_vertex, max_weights);

            if (max_weights > 0) {
                // Reserve space in skin_attribs
                size_t start_idx = skin_attribs.size();
                skin_attribs.resize(start_idx + vertex_skin_attribs.size() * max_weights);

                for (size_t v = 0; v < vertex_skin_attribs.size(); ++v) {
                    auto &attribs = vertex_skin_attribs[v];

                    // Normalize weights
                    float total_weight = 0.0f;
                    for (auto &a : attribs) {
                        total_weight += a.weight;
                    }
                    if (total_weight > 0.0f) {
                        for (auto &a : attribs) {
                            a.weight /= total_weight;
                        }
                    }

                    // Copy to skin_attribs
                    for (size_t w = 0; w < max_weights; ++w) {
                        if (w < attribs.size()) {
                            skin_attribs[start_idx + v * max_weights + w] = attribs[w];
                        } else {
                            skin_attribs[start_idx + v * max_weights + w] = SkinAttrib{0, 0.0f};
                        }
                    }
                }
            }
        }
    }

    scene->destroy();

    if (mesh_builder.position.empty()) {
        LUISA_WARNING("No valid geometry found in FBX file: {}", path.string());
        return false;
    }

    // Calculate tangents if UVs and normals are available
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
        } else if (!mesh_builder.triangle_indices.empty()) {
            calculate_tangent(mesh_builder.position,
                              mesh_builder.uvs[0],
                              mesh_builder.tangent,
                              luisa::span{(Triangle const *)(mesh_builder.triangle_indices[0].data()),
                                          mesh_builder.triangle_indices[0].size() / 3},
                              1);
        }
    }

    // Write mesh data to resource
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(std::move(submesh_offsets),
                           mesh_builder.vertex_count(),
                           mesh_builder.indices_count() / 3,
                           mesh_builder.uv_count(),
                           mesh_builder.contained_normal(),
                           mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);

    // Add skinning property if available
    if (max_skin_weights_per_vertex > 0 && !skin_attribs.empty()) {
        auto property = resource->add_property("skin_attrib",
                                               max_skin_weights_per_vertex *
                                                   resource->vertex_count() *
                                                   sizeof(SkinAttrib));

        auto skin_span = luisa::span{(SkinAttrib *)property.second.data(), property.second.size()};

        std::memset(skin_span.data(), 0, skin_span.size_bytes());
        std::memcpy(skin_span.data(),
                    skin_attribs.data(),
                    std::min(skin_span.size_bytes(),
                             skin_attribs.size() * sizeof(SkinAttrib)));
    }

    return true;
}

}// namespace rbc::world
