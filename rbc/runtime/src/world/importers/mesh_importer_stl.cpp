#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_stl.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <cstring>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

namespace {
    // Binary STL header size (80 bytes)
    constexpr size_t STL_HEADER_SIZE = 80;
    // Size of one triangle in binary STL (12 bytes normal + 36 bytes vertices + 2 bytes attribute)
    constexpr size_t STL_TRIANGLE_SIZE = 50;
    // Maximum reasonable file size for ASCII check
    constexpr size_t STL_ASCII_CHECK_SIZE = 512;
}

bool StlMeshImporter::_is_binary_format(luisa::span<const std::byte> data) const {
    if (data.size() < STL_HEADER_SIZE + 4) {
        return false;  // Too small to be valid binary STL
    }

    // Get triangle count from binary header
    uint32_t triangle_count;
    std::memcpy(&triangle_count, data.data() + STL_HEADER_SIZE, sizeof(uint32_t));

    // Calculate expected file size for binary format
    size_t expected_binary_size = STL_HEADER_SIZE + 4 + triangle_count * STL_TRIANGLE_SIZE;

    // If file size matches binary format, it's likely binary
    // Also check for "solid " at start which indicates ASCII
    if (data.size() == expected_binary_size) {
        // Check if it starts with "solid " (case insensitive for safety)
        auto is_ascii_prefix = [&]() -> bool {
            std::string_view header_str(reinterpret_cast<const char *>(data.data()), 
                                      std::min(size_t{6}, data.size()));
            return header_str.size() >= 6 && 
                   (header_str[0] == 's' || header_str[0] == 'S') &&
                   (header_str[1] == 'o' || header_str[1] == 'O') &&
                   (header_str[2] == 'l' || header_str[2] == 'L') &&
                   (header_str[3] == 'i' || header_str[3] == 'I') &&
                   (header_str[4] == 'd' || header_str[4] == 'D') &&
                   (header_str[5] == ' ');
        };
        
        // If it matches binary size but has "solid " prefix, it's probably ASCII
        // (some binary files may coincidentally match size)
        if (is_ascii_prefix()) {
            return false;
        }
        return true;
    }

    return false;
}

float3 StlMeshImporter::_compute_normal(
    const float3 &v0,
    const float3 &v1,
    const float3 &v2) const {
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 n = cross(edge1, edge2);
    float len = length(n);
    if (len > 0.0f) {
        return n / len;
    }
    return make_float3(0.0f, 1.0f, 0.0f);  // Default normal for degenerate triangles
}

bool StlMeshImporter::_import_binary(
    luisa::span<const std::byte> data,
    MeshBuilder &mesh_builder) const {
    if (data.size() < STL_HEADER_SIZE + 4) {
        return false;
    }

    // Read triangle count from header
    uint32_t triangle_count;
    std::memcpy(&triangle_count, data.data() + STL_HEADER_SIZE, sizeof(uint32_t));

    if (triangle_count == 0) {
        LUISA_WARNING("STL file contains no triangles");
        return false;
    }

    // Check data size
    size_t expected_size = STL_HEADER_SIZE + 4 + triangle_count * STL_TRIANGLE_SIZE;
    if (data.size() < expected_size) {
        LUISA_WARNING("STL binary file is truncated");
        return false;
    }

    // Reserve space for vertices
    mesh_builder.position.reserve(triangle_count * 3);
    mesh_builder.normal.reserve(triangle_count * 3);

    auto &indices = mesh_builder.triangle_indices.emplace_back();
    indices.reserve(triangle_count * 3);

    // Read triangles starting after header + count
    const std::byte *ptr = data.data() + STL_HEADER_SIZE + 4;
    for (uint32_t i = 0; i < triangle_count; ++i) {
        // Read normal (3 floats)
        float normal_data[3];
        std::memcpy(normal_data, ptr, sizeof(normal_data));
        float3 face_normal = make_float3(normal_data[0], normal_data[1], normal_data[2]);
        ptr += 12;

        // Read vertices (9 floats: 3 vertices * 3 coordinates)
        float vertex_data[9];
        std::memcpy(vertex_data, ptr, sizeof(vertex_data));
        ptr += 36;

        // Skip attribute byte count (2 bytes)
        ptr += 2;

        uint32_t base_index = static_cast<uint32_t>(mesh_builder.position.size());

        // Add 3 vertices
        for (int v = 0; v < 3; ++v) {
            float3 pos = make_float3(
                vertex_data[v * 3 + 0],
                vertex_data[v * 3 + 1],
                vertex_data[v * 3 + 2]);
            mesh_builder.position.push_back(pos);
            mesh_builder.normal.push_back(face_normal);
        }

        // Add triangle indices (flip winding order: STL uses outward-facing, we need CCW)
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 1);
    }

    return true;
}

bool StlMeshImporter::_import_ascii(
    luisa::span<const std::byte> data,
    MeshBuilder &mesh_builder) const {
    // Convert data to string view
    std::string_view content(reinterpret_cast<const char *>(data.data()), data.size());

    // Parse ASCII STL format
    // Format: solid name
    //   facet normal nx ny nz
    //     outer loop
    //       vertex vx vy vz
    //       vertex vx vy vz
    //       vertex vx vy vz
    //     endloop
    //   endfacet
    // endsolid name

    size_t pos = 0;

    // Helper to skip whitespace
    auto skip_whitespace = [&](size_t &p) {
        while (p < content.size() && 
               (content[p] == ' ' || content[p] == '\t' || 
                content[p] == '\n' || content[p] == '\r')) {
            ++p;
        }
    };

    // Helper to find next token
    auto find_token = [&](size_t &p, std::string_view token) -> bool {
        skip_whitespace(p);
        if (p + token.size() <= content.size()) {
            std::string_view sub(content.data() + p, token.size());
            // Case insensitive compare
            bool match = true;
            for (size_t i = 0; i < token.size(); ++i) {
                if (std::tolower(sub[i]) != token[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                p += token.size();
                return true;
            }
        }
        return false;
    };

    // Helper to parse 3 floats
    auto parse_float3 = [&](size_t &p, float3 &out) -> bool {
        skip_whitespace(p);
        // Parse first float
        const char *start = content.data() + p;
        char *end;
        out.x = std::strtof(start, &end);
        if (start == end) return false;
        p = end - content.data();

        // Parse second float
        skip_whitespace(p);
        start = content.data() + p;
        out.y = std::strtof(start, &end);
        if (start == end) return false;
        p = end - content.data();

        // Parse third float
        skip_whitespace(p);
        start = content.data() + p;
        out.z = std::strtof(start, &end);
        if (start == end) return false;
        p = end - content.data();

        return true;
    };

    // Skip "solid" line
    skip_whitespace(pos);
    if (!find_token(pos, "solid")) {
        LUISA_WARNING("Invalid ASCII STL: missing 'solid' keyword");
        return false;
    }
    
    // Skip solid name (until newline)
    while (pos < content.size() && content[pos] != '\n' && content[pos] != '\r') {
        ++pos;
    }

    auto &indices = mesh_builder.triangle_indices.emplace_back();

    // Parse facets
    while (pos < content.size()) {
        skip_whitespace(pos);
        
        // Check for endsolid
        if (find_token(pos, "endsolid")) {
            break;
        }

        // Expect "facet normal"
        if (!find_token(pos, "facet")) {
            continue;  // Skip unknown content
        }
        
        if (!find_token(pos, "normal")) {
            LUISA_WARNING("Invalid ASCII STL: expected 'normal' after 'facet'");
            return false;
        }

        // Parse normal
        float3 normal;
        if (!parse_float3(pos, normal)) {
            LUISA_WARNING("Invalid ASCII STL: failed to parse normal");
            return false;
        }

        // Expect "outer loop"
        if (!find_token(pos, "outer")) {
            LUISA_WARNING("Invalid ASCII STL: expected 'outer'");
            return false;
        }
        if (!find_token(pos, "loop")) {
            LUISA_WARNING("Invalid ASCII STL: expected 'loop'");
            return false;
        }

        // Parse 3 vertices
        float3 vertices[3];
        for (int i = 0; i < 3; ++i) {
            if (!find_token(pos, "vertex")) {
                LUISA_WARNING("Invalid ASCII STL: expected 'vertex'");
                return false;
            }
            if (!parse_float3(pos, vertices[i])) {
                LUISA_WARNING("Invalid ASCII STL: failed to parse vertex");
                return false;
            }
        }

        // Expect "endloop"
        if (!find_token(pos, "endloop")) {
            LUISA_WARNING("Invalid ASCII STL: expected 'endloop'");
            return false;
        }

        // Expect "endfacet"
        if (!find_token(pos, "endfacet")) {
            LUISA_WARNING("Invalid ASCII STL: expected 'endfacet'");
            return false;
        }

        // Add triangle to mesh
        uint32_t base_index = static_cast<uint32_t>(mesh_builder.position.size());

        for (int i = 0; i < 3; ++i) {
            mesh_builder.position.push_back(vertices[i]);
            mesh_builder.normal.push_back(normal);
        }

        // Add indices (flip winding order)
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 1);
    }

    return !mesh_builder.position.empty();
}

bool StlMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) {
        LUISA_WARNING("Failed to open STL file: {}", luisa::to_string(path));
        return false;
    }

    // Read entire file into memory
    size_t file_size = file_stream.length();
    luisa::vector<std::byte> file_data;
    file_data.resize(file_size);
    file_stream.read({file_data.data(), file_size});

    MeshBuilder mesh_builder;
    bool success = false;

    // Determine format and import
    if (_is_binary_format(file_data)) {
        success = _import_binary(file_data, mesh_builder);
    } else {
        success = _import_ascii(file_data, mesh_builder);
    }

    if (!success || mesh_builder.position.empty()) {
        LUISA_WARNING("Failed to import STL file: {}", luisa::to_string(path));
        return false;
    }

    // Size check: ensure all vertex attributes have the same size as positions
    const size_t position_size = mesh_builder.position.size();
    if (mesh_builder.normal.size() != position_size) {
        mesh_builder.normal.resize(position_size);
    }
    if (mesh_builder.tangent.size() != position_size) {
        mesh_builder.tangent.resize(position_size);
    }
    for (auto &uv : mesh_builder.uvs) {
        if (uv.size() != position_size) {
            uv.resize(position_size);
        }
    }

    // Generate submesh offsets and create resource
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
