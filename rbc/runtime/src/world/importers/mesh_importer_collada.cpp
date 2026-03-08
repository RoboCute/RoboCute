#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_collada.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/rtx/triangle.h>
#include <tinyxml2.h>
#include <cstring>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

/**
 * @brief Parse float array from COLLADA source element
 * @param source_elem The source XML element
 * @param float_array Output float array
 * @return true if successful
 */
static bool parse_float_array(tinyxml2::XMLElement *source_elem, luisa::vector<float> &float_array) {
    auto *float_array_elem = source_elem->FirstChildElement("float_array");
    if (!float_array_elem) {
        float_array_elem = source_elem->FirstChildElement("FloatArray");
    }
    if (!float_array_elem) return false;

    const char *text = float_array_elem->GetText();
    if (!text) return false;

    size_t count = 0;
    float_array_elem->QueryUnsigned64Attribute("count", &count);
    if (count == 0) return false;

    float_array.reserve(count);
    std::istringstream iss(text);
    float value;
    while (iss >> value) {
        float_array.push_back(value);
    }
    return !float_array.empty();
}

/**
 * @brief Parse vertex indices from triangles or polylist element
 * @param primitive_elem The triangles/polylist XML element
 * @param indices Output index array
 * @param offset_map Map of semantic to input offset
 * @param vertex_offset The offset for VERTEX semantic
 * @return true if successful
 */
static bool parse_indices(
    tinyxml2::XMLElement *primitive_elem,
    luisa::vector<uint> &indices,
    vstd::HashMap<luisa::string, uint> &offset_map,
    uint &vertex_offset) {
    const char *count_str = primitive_elem->Attribute("count");
    if (!count_str) return false;

    uint triangle_count = static_cast<uint>(std::atoi(count_str));
    if (triangle_count == 0) return false;

    // Parse input elements to get offsets
    uint max_offset = 0;
    for (auto *input = primitive_elem->FirstChildElement("input");
         input;
         input = input->NextSiblingElement("input")) {
        const char *semantic = input->Attribute("semantic");
        const char *offset_str = input->Attribute("offset");
        if (!semantic || !offset_str) continue;

        uint offset = static_cast<uint>(std::atoi(offset_str));
        offset_map.emplace(semantic, offset);
        max_offset = std::max(max_offset, offset);

        if (std::strcmp(semantic, "VERTEX") == 0) {
            vertex_offset = offset;
        }
    }

    uint stride = max_offset + 1;

    // Check if we have <p> element (triangles) or <vcount>+<p> (polylist)
    auto *p_elem = primitive_elem->FirstChildElement("p");
    if (!p_elem) return false;

    const char *text = p_elem->GetText();
    if (!text) return false;

    // Parse all indices
    luisa::vector<uint> all_indices;
    std::istringstream iss(text);
    uint value;
    while (iss >> value) {
        all_indices.push_back(value);
    }

    // Extract vertex indices
    indices.reserve(triangle_count * 3);
    for (size_t i = vertex_offset; i < all_indices.size(); i += stride) {
        indices.push_back(all_indices[i]);
    }

    return !indices.empty();
}

/**
 * @brief Parse mesh geometry from COLLADA XML
 * @param mesh_elem The mesh XML element
 * @param mesh_builder Output mesh builder
 * @return true if successful
 */
static bool parse_mesh(tinyxml2::XMLElement *mesh_elem, MeshBuilder &mesh_builder) {
    // Map of source IDs to float arrays
    vstd::HashMap<luisa::string, luisa::vector<float>> sources;

    // Parse all source elements
    for (auto *source = mesh_elem->FirstChildElement("source");
         source;
         source = source->NextSiblingElement("source")) {
        const char *id = source->Attribute("id");
        if (!id) continue;

        luisa::vector<float> data;
        if (parse_float_array(source, data)) {
            sources.emplace(id, std::move(data));
        }
    }

    // Find vertices element and its position source
    auto *vertices_elem = mesh_elem->FirstChildElement("vertices");
    luisa::string position_source_id;
    if (vertices_elem) {
        for (auto *input = vertices_elem->FirstChildElement("input");
             input;
             input = input->NextSiblingElement("input")) {
            const char *semantic = input->Attribute("semantic");
            const char *source = input->Attribute("source");
            if (semantic && source && std::strcmp(semantic, "POSITION") == 0) {
                // Remove leading #
                if (source[0] == '#') {
                    position_source_id = source + 1;
                } else {
                    position_source_id = source;
                }
                break;
            }
        }
    }

    // Get position data
    auto position_it = sources.find(position_source_id);
    if (!position_it) {
        LUISA_WARNING("COLLADA mesh has no position data");
        return false;
    }

    auto &positions = position_it.value();
    mesh_builder.position.reserve(positions.size() / 3);
    for (size_t i = 0; i + 2 < positions.size(); i += 3) {
        mesh_builder.position.push_back(make_float3(
            positions[i], positions[i + 1], positions[i + 2]));
    }

    // Find triangles or polylist elements
    tinyxml2::XMLElement *primitive_elem = nullptr;
    bool is_triangles = true;

    for (auto *triangles = mesh_elem->FirstChildElement("triangles");
         triangles;
         triangles = triangles->NextSiblingElement("triangles")) {
        primitive_elem = triangles;
        break;
    }

    if (!primitive_elem) {
        for (auto *polylist = mesh_elem->FirstChildElement("polylist");
             polylist;
             polylist = polylist->NextSiblingElement("polylist")) {
            primitive_elem = polylist;
            is_triangles = false;
            break;
        }
    }

    if (!primitive_elem) {
        // Try <lines> or other primitives - not supported for now
        LUISA_WARNING("COLLADA mesh has no triangles or polylist");
        return false;
    }

    // Parse triangle indices
    luisa::vector<uint> vertex_indices;
    vstd::HashMap<luisa::string, uint> offset_map;
    uint vertex_offset = 0;

    if (!parse_indices(primitive_elem, vertex_indices, offset_map, vertex_offset)) {
        LUISA_WARNING("Failed to parse COLLADA indices");
        return false;
    }

    // Store triangle indices
    auto &ind = mesh_builder.triangle_indices.emplace_back();
    ind = std::move(vertex_indices);

    // Try to find and parse normal data
    luisa::string normal_source_id;
    uint normal_offset = 0;
    bool has_normals = false;

    for (auto *input = primitive_elem->FirstChildElement("input");
         input;
         input = input->NextSiblingElement("input")) {
        const char *semantic = input->Attribute("semantic");
        const char *source = input->Attribute("source");
        const char *offset_str = input->Attribute("offset");
        if (semantic && source && std::strcmp(semantic, "NORMAL") == 0) {
            if (source[0] == '#') {
                normal_source_id = source + 1;
            } else {
                normal_source_id = source;
            }
            if (offset_str) {
                normal_offset = static_cast<uint>(std::atoi(offset_str));
            }
            has_normals = true;
            break;
        }
    }

    if (has_normals) {
        auto normal_it = sources.find(normal_source_id);
        if (normal_it) {
            auto &normals = normal_it.value();
            mesh_builder.normal.reserve(normals.size() / 3);
            for (size_t i = 0; i + 2 < normals.size(); i += 3) {
                mesh_builder.normal.push_back(make_float3(
                    normals[i], normals[i + 1], normals[i + 2]));
            }
        }
    }

    // Try to find and parse UV data
    luisa::string uv_source_id;
    uint uv_offset = 0;
    bool has_uvs = false;

    for (auto *input = primitive_elem->FirstChildElement("input");
         input;
         input = input->NextSiblingElement("input")) {
        const char *semantic = input->Attribute("semantic");
        const char *source = input->Attribute("source");
        const char *offset_str = input->Attribute("offset");
        if (semantic && source &&
            (std::strcmp(semantic, "TEXCOORD") == 0 ||
             std::strcmp(semantic, "UV") == 0)) {
            if (source[0] == '#') {
                uv_source_id = source + 1;
            } else {
                uv_source_id = source;
            }
            if (offset_str) {
                uv_offset = static_cast<uint>(std::atoi(offset_str));
            }
            has_uvs = true;
            break;
        }
    }

    if (has_uvs) {
        auto uv_it = sources.find(uv_source_id);
        if (uv_it) {
            auto &uvs = uv_it.value();
            auto &uv_array = mesh_builder.uvs.emplace_back();
            uv_array.reserve(uvs.size() / 2);
            for (size_t i = 0; i + 1 < uvs.size(); i += 2) {
                uv_array.push_back(make_float2(uvs[i], uvs[i + 1]));
            }
        }
    }

    return !mesh_builder.position.empty() && !mesh_builder.triangle_indices.empty();
}

bool ColladaMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        LUISA_WARNING("Failed to load COLLADA file: {}", path.string());
        return false;
    }

    auto *root = doc.RootElement();
    if (!root || std::strcmp(root->Name(), "COLLADA") != 0) {
        LUISA_WARNING("Invalid COLLADA file format: {}", path.string());
        return false;
    }

    // Find library_geometries
    auto *lib_geometries = root->FirstChildElement("library_geometries");
    if (!lib_geometries) {
        LUISA_WARNING("COLLADA file has no library_geometries: {}", path.string());
        return false;
    }

    MeshBuilder mesh_builder;
    bool found_mesh = false;

    // Parse all geometry elements
    for (auto *geometry = lib_geometries->FirstChildElement("geometry");
         geometry;
         geometry = geometry->NextSiblingElement("geometry")) {
        auto *mesh_elem = geometry->FirstChildElement("mesh");
        if (!mesh_elem) continue;

        if (parse_mesh(mesh_elem, mesh_builder)) {
            found_mesh = true;
            // For now, only parse the first mesh
            break;
        }
    }

    if (!found_mesh) {
        LUISA_WARNING("No valid mesh found in COLLADA file: {}", path.string());
        return false;
    }

    // Calculate tangents if we have UVs and normals
    if (mesh_builder.uv_count() > 0 && mesh_builder.normal.size() > 0) {
        mesh_builder.tangent.push_back_uninitialized(mesh_builder.vertex_count());
        if (mesh_builder.triangle_indices.size() > 0) {
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

    // Write mesh data to resource
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(
        std::move(submesh_offsets),
        mesh_builder.vertex_count(),
        mesh_builder.indices_count() / 3,
        mesh_builder.uv_count(),
        mesh_builder.contained_normal(),
        mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);


    return true;
}

}// namespace rbc::world
