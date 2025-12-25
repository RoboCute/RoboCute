#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <tiny_gltf.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <cstring>
#include <map>
#include <algorithm>
#include "rbc_world/importers/gltf_scene_loader.h"

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

// Helper function to read accessor data
template<typename T>
static bool read_accessor_data(
    tinygltf::Model const &model,
    int accessor_index,
    luisa::vector<T> &out_data) {
    if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    auto const &accessor = model.accessors[accessor_index];
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    auto const &buffer_view = model.bufferViews[accessor.bufferView];
    if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
        return false;
    }

    auto const &buffer = model.buffers[buffer_view.buffer];

    size_t component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    int num_components = tinygltf::GetNumComponentsInType(accessor.type);
    if (component_size <= 0 || num_components <= 0) {
        return false;
    }

    size_t element_size = component_size * num_components;
    size_t byte_stride = buffer_view.byteStride > 0 ? buffer_view.byteStride : element_size;

    size_t data_offset = buffer_view.byteOffset + accessor.byteOffset;
    size_t data_size = accessor.count * element_size;

    if (data_offset + data_size > buffer.data.size()) {
        return false;
    }

    out_data.reserve(accessor.count);

    // Read and convert data
    for (size_t i = 0; i < accessor.count; ++i) {
        size_t byte_offset = data_offset + i * byte_stride;
        uint8_t const *data_ptr = buffer.data.data() + byte_offset;

        if constexpr (std::is_same_v<T, float3>) {
            if (num_components == 3 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1], float_ptr[2]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, float2>) {
            if (num_components == 2 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, float4>) {
            if (num_components == 4 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1], float_ptr[2], float_ptr[3]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, uint>) {
            if (num_components == 1) {
                uint32_t index_value = 0;
                switch (accessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index_value = static_cast<uint32_t>(*reinterpret_cast<uint8_t const *>(data_ptr));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index_value = static_cast<uint32_t>(*reinterpret_cast<uint16_t const *>(data_ptr));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index_value = *reinterpret_cast<uint32_t const *>(data_ptr);
                        break;
                    default:
                        return false;
                }
                out_data.push_back(index_value);
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

// Helper function to read accessor data as raw bytes (for skin weights, colors, etc.)
static bool read_accessor_raw(
    tinygltf::Model const &model,
    int accessor_index,
    luisa::vector<std::byte> &out_data) {
    if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    auto const &accessor = model.accessors[accessor_index];
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    auto const &buffer_view = model.bufferViews[accessor.bufferView];
    if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
        return false;
    }

    auto const &buffer = model.buffers[buffer_view.buffer];

    size_t component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    int num_components = tinygltf::GetNumComponentsInType(accessor.type);
    if (component_size <= 0 || num_components <= 0) {
        return false;
    }

    size_t element_size = component_size * num_components;
    size_t byte_stride = buffer_view.byteStride > 0 ? buffer_view.byteStride : element_size;

    size_t data_offset = buffer_view.byteOffset + accessor.byteOffset;
    size_t data_size = accessor.count * element_size;

    if (data_offset + data_size > buffer.data.size()) {
        return false;
    }

    out_data.resize(data_size);

    // Copy data from unsigned char buffer to std::byte vector
    if (byte_stride == element_size) {
        // Tightly packed - single memcpy
        std::memcpy(out_data.data(), buffer.data.data() + data_offset, data_size);
    } else {
        // Strided - copy element by element
        std::byte *out_ptr = out_data.data();
        for (size_t i = 0; i < accessor.count; ++i) {
            size_t byte_offset = data_offset + i * byte_stride;
            std::memcpy(out_ptr, buffer.data.data() + byte_offset, element_size);
            out_ptr += element_size;
        }
    }

    return true;
}

// Helper function to process GLTF model and build MeshBuilder
// Returns MeshBuilder and additional data (skin weights, vertex colors)
struct GltfImportData {
    MeshBuilder mesh_builder;
    size_t max_weight_count = 0;// joint weight suite
    luisa::vector<SkinAttrib> all_skin_weights;
    luisa::vector<float> all_vertex_colors;
    size_t vertex_color_channels = 0;
};

static GltfImportData process_gltf_model(tinygltf::Model const &model) {
    GltfImportData result;
    MeshBuilder &mesh_builder = result.mesh_builder;

    // Process all meshes and primitives
    for (auto const &mesh : model.meshes) {
        for (auto const &primitive : mesh.primitives) {
            // Only process triangle primitives
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) {
                continue;
            }

            // Read positions (required)
            auto pos_it = primitive.attributes.find("POSITION");
            if (pos_it == primitive.attributes.end()) {
                continue;// Skip primitives without positions
            }

            luisa::vector<float3> positions;
            if (!read_accessor_data(model, pos_it->second, positions)) {
                continue;
            }

            size_t vertex_count = positions.size();
            if (vertex_count == 0) {
                continue;
            }

            // Read normals (optional)
            luisa::vector<float3> normals;
            auto normal_it = primitive.attributes.find("NORMAL");
            if (normal_it != primitive.attributes.end()) {
                read_accessor_data(model, normal_it->second, normals);
            }

            // Read UVs (optional, support multiple UV sets)
            std::map<int, luisa::vector<float2>> uv_sets;
            for (int uv_idx = 0; uv_idx < 8; ++uv_idx) {
                std::string uv_name = uv_idx == 0 ? "TEXCOORD_0" :
                                                    "TEXCOORD_" + std::to_string(uv_idx);
                auto uv_it = primitive.attributes.find(uv_name);
                if (uv_it != primitive.attributes.end()) {
                    luisa::vector<float2> uvs;
                    if (read_accessor_data(model, uv_it->second, uvs)) {
                        uv_sets[uv_idx] = std::move(uvs);
                    }
                }
            }

            // Read tangents (optional)
            luisa::vector<float4> tangents;
            auto tangent_it = primitive.attributes.find("TANGENT");
            if (tangent_it != primitive.attributes.end()) {
                luisa::vector<float4> tangent_data;
                if (read_accessor_data(model, tangent_it->second, tangent_data)) {
                    tangents = std::move(tangent_data);
                }
            }

            // Read indices
            luisa::vector<uint> indices;
            if (primitive.indices >= 0) {
                read_accessor_data(model, primitive.indices, indices);
            } else {
                // Generate indices if not provided
                indices.reserve(vertex_count);
                for (uint i = 0; i < static_cast<uint>(vertex_count); ++i) {
                    indices.push_back(i);
                }
            }

            // Ensure indices are triangles (should be already, but check)
            if (indices.size() % 3 != 0) {
                continue;
            }

            // Determine vertex offset for this primitive
            size_t vertex_offset = mesh_builder.position.size();

            // Add positions
            mesh_builder.position.insert(mesh_builder.position.end(), positions.begin(), positions.end());

            // Add normals
            if (!normals.empty() && normals.size() == vertex_count) {
                if (mesh_builder.normal.empty()) {
                    mesh_builder.normal.resize(vertex_offset);
                }
                mesh_builder.normal.insert(mesh_builder.normal.end(), normals.begin(), normals.end());
            }

            // Add UVs
            for (auto const &[uv_idx, uvs] : uv_sets) {
                if (uvs.size() == vertex_count) {
                    while (mesh_builder.uvs.size() <= static_cast<size_t>(uv_idx)) {
                        mesh_builder.uvs.emplace_back();
                        if (mesh_builder.uvs.size() > 1) {
                            mesh_builder.uvs.back().resize(vertex_offset);
                        }
                    }
                    mesh_builder.uvs[uv_idx].insert(
                        mesh_builder.uvs[uv_idx].end(), uvs.begin(), uvs.end());
                }
            }

            // Add tangents
            if (!tangents.empty() && tangents.size() == vertex_count) {
                if (mesh_builder.tangent.empty()) {
                    mesh_builder.tangent.resize(vertex_offset);
                }
                mesh_builder.tangent.insert(mesh_builder.tangent.end(), tangents.begin(), tangents.end());
            } else if (!mesh_builder.tangent.empty()) {
                // If tangent array already exists but this primitive has no tangents,
                // add default tangents to maintain array size consistency
                mesh_builder.tangent.insert(mesh_builder.tangent.end(), vertex_count, float4(0, 0, 0, 1));
            }

            // Add indices (with vertex offset)
            auto &triangle_indices = mesh_builder.triangle_indices.emplace_back();
            triangle_indices.reserve(indices.size());
            for (auto idx : indices) {
                triangle_indices.push_back(static_cast<uint>(vertex_offset + idx));
            }
        }
    }

    if (mesh_builder.position.empty()) {
        return result;// Return empty result
    }

    // Calculate tangents if we have UVs and normals but no tangents
    if (mesh_builder.tangent.empty() && mesh_builder.uv_count() > 0 && !mesh_builder.normal.empty()) {
        mesh_builder.tangent.resize(mesh_builder.vertex_count());
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

    // Ensure tangent array size matches position array size
    // If tangents exist but are incomplete, recalculate all tangents to ensure consistency
    if (!mesh_builder.tangent.empty() && mesh_builder.tangent.size() != mesh_builder.vertex_count()) {
        if (mesh_builder.uv_count() > 0 && !mesh_builder.normal.empty()) {
            // Recalculate all tangents if we have UVs and normals
            mesh_builder.tangent.resize(mesh_builder.vertex_count());
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
                calculate_tangent(
                    mesh_builder.position,
                    mesh_builder.uvs[0],
                    mesh_builder.tangent,
                    luisa::span{
                        (Triangle const *)(mesh_builder.triangle_indices[0].data()),
                        mesh_builder.triangle_indices[0].size() / 3},
                    1);
            }
        } else {
            // Fill missing tangents with default values if we can't calculate them
            mesh_builder.tangent.resize(mesh_builder.vertex_count(), float4(0, 0, 0, 1));
        }
    }

    // First pass: determine max weight count
    size_t max_weight_count = 0;
    for (auto const &mesh : model.meshes) {
        for (auto const &primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) {
                continue;
            }

            auto joints_it = primitive.attributes.find("JOINTS_0");
            auto weights_it = primitive.attributes.find("WEIGHTS_0");

            if (joints_it != primitive.attributes.end() && weights_it != primitive.attributes.end()) {
                auto const &joints_accessor = model.accessors[joints_it->second];
                auto const &weights_accessor = model.accessors[weights_it->second];
                int joint_num_components = tinygltf::GetNumComponentsInType(joints_accessor.type);
                int weight_num_components = tinygltf::GetNumComponentsInType(weights_accessor.type);

                size_t weight_count = std::min(joint_num_components, weight_num_components);
                max_weight_count = std::max(max_weight_count, static_cast<size_t>(weight_count));
            }
        }
    }

    // Second pass: process skinning weights and vertex colors
    luisa::vector<SkinAttrib> all_skin_weights;

    luisa::vector<float> all_vertex_colors;

    size_t vertex_color_channels = 0;

    size_t current_vertex_offset = 0;
    for (auto const &mesh : model.meshes) {
        for (auto const &primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) {
                continue;
            }

            auto pos_it = primitive.attributes.find("POSITION");
            if (pos_it == primitive.attributes.end()) {
                continue;
            }

            auto const &pos_accessor = model.accessors[pos_it->second];
            size_t primitive_vertex_count = pos_accessor.count;

            // Verify that this matches the vertex count we added to mesh_builder
            if (current_vertex_offset + primitive_vertex_count > mesh_builder.position.size()) {
                continue;// Skip if mismatch
            }

            // Read skinning weights
            auto joints_it = primitive.attributes.find("JOINTS_0");
            auto weights_it = primitive.attributes.find("WEIGHTS_0");

            if (joints_it != primitive.attributes.end() && weights_it != primitive.attributes.end()) {
                luisa::vector<std::byte> joints_data;
                luisa::vector<std::byte> weights_data;

                if (read_accessor_raw(model, joints_it->second, joints_data) &&
                    read_accessor_raw(model, weights_it->second, weights_data)) {
                    auto const &joints_accessor = model.accessors[joints_it->second];
                    auto const &weights_accessor = model.accessors[weights_it->second];

                    size_t joint_component_size = tinygltf::GetComponentSizeInBytes(joints_accessor.componentType);
                    int joint_num_components = tinygltf::GetNumComponentsInType(joints_accessor.type);
                    size_t weight_component_size = tinygltf::GetComponentSizeInBytes(weights_accessor.componentType);
                    int weight_num_components = tinygltf::GetNumComponentsInType(weights_accessor.type);

                    size_t weight_count = std::min(joint_num_components, weight_num_components);

                    // Resize skin weights array if needed (store max_weight_count per vertex)
                    size_t total_weights_needed = (current_vertex_offset + primitive_vertex_count) * max_weight_count;

                    if (all_skin_weights.size() < total_weights_needed) {
                        all_skin_weights.resize(total_weights_needed);
                    }

                    for (size_t v = 0; v < primitive_vertex_count; ++v) {
                        size_t joint_offset = v * joint_num_components * joint_component_size;
                        size_t weight_offset = v * weight_num_components * weight_component_size;
                        size_t base_idx = (current_vertex_offset + v) * max_weight_count;

                        for (size_t w = 0; w < weight_count && w < max_weight_count; ++w) {
                            int32_t joint_id = 0;
                            float weight = 0.0f;

                            // Read joint ID
                            if (joints_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                                joint_id = static_cast<int32_t>(joints_data[joint_offset + w * joint_component_size]);
                            } else if (joints_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                                joint_id = static_cast<int32_t>(*reinterpret_cast<uint16_t const *>(
                                    joints_data.data() + joint_offset + w * joint_component_size));
                            } else if (joints_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                                joint_id = static_cast<int32_t>(*reinterpret_cast<uint32_t const *>(
                                    joints_data.data() + joint_offset + w * joint_component_size));
                            }

                            // Read weight
                            if (weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                                weight = *reinterpret_cast<float const *>(
                                    weights_data.data() + weight_offset + w * weight_component_size);
                            } else if (weights_accessor.normalized) {
                                // Normalized integer weights
                                if (weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                                    weight = static_cast<float>(weights_data[weight_offset + w * weight_component_size]) / 255.0f;
                                } else if (weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                                    weight = static_cast<float>(*reinterpret_cast<uint16_t const *>(
                                                 weights_data.data() + weight_offset + w * weight_component_size)) /
                                             65535.0f;
                                }
                            }

                            if (base_idx + w < all_skin_weights.size()) {
                                all_skin_weights[base_idx + w].joint_id = joint_id;
                                all_skin_weights[base_idx + w].weight = weight;
                            }
                        }
                    }
                }
            }

            // Read vertex colors
            auto color_it = primitive.attributes.find("COLOR_0");
            if (color_it != primitive.attributes.end()) {
                luisa::vector<std::byte> color_data;
                if (read_accessor_raw(model, color_it->second, color_data)) {
                    auto const &color_accessor = model.accessors[color_it->second];
                    int color_num_components = tinygltf::GetNumComponentsInType(color_accessor.type);
                    size_t color_component_size = tinygltf::GetComponentSizeInBytes(color_accessor.componentType);

                    vertex_color_channels = std::max(vertex_color_channels, static_cast<size_t>(color_num_components));

                    // Convert color data to float array
                    size_t color_data_size = primitive_vertex_count * color_num_components * sizeof(float);
                    if (all_vertex_colors.size() < (current_vertex_offset + primitive_vertex_count) * color_num_components) {
                        all_vertex_colors.resize((current_vertex_offset + primitive_vertex_count) * color_num_components);
                    }

                    for (size_t v = 0; v < primitive_vertex_count; ++v) {
                        size_t color_offset = v * color_num_components * color_component_size;
                        size_t output_offset = (current_vertex_offset + v) * color_num_components;

                        for (int c = 0; c < color_num_components; ++c) {
                            float color_value = 0.0f;
                            if (color_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                                color_value = *reinterpret_cast<float const *>(
                                    color_data.data() + color_offset + c * color_component_size);
                            } else if (color_accessor.normalized) {
                                if (color_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                                    color_value = static_cast<float>(color_data[color_offset + c * color_component_size]) / 255.0f;
                                } else if (color_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                                    color_value = static_cast<float>(*reinterpret_cast<uint16_t const *>(
                                                      color_data.data() + color_offset + c * color_component_size)) /
                                                  65535.0f;
                                }
                            }
                            all_vertex_colors[output_offset + c] = color_value;
                        }
                    }
                }
            }

            current_vertex_offset += primitive_vertex_count;
        }
    }

    // Store the processed data
    result.max_weight_count = max_weight_count;
    result.all_skin_weights = std::move(all_skin_weights);
    result.all_vertex_colors = std::move(all_vertex_colors);
    result.vertex_color_channels = vertex_color_channels;

    return result;
}

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
