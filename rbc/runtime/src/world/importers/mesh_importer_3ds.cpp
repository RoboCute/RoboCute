#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_3ds.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <cstring>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

// 3DS file format chunk IDs
namespace chunk_id {
constexpr uint16_t MAIN_CHUNK = 0x4D4D;
constexpr uint16_t VERSION = 0x0002;
constexpr uint16_t EDITOR_CHUNK = 0x3D3D;
constexpr uint16_t EDITOR_VERSION = 0x3D3E;
constexpr uint16_t OBJECT_BLOCK = 0x4000;
constexpr uint16_t TRIANGULAR_MESH = 0x4100;
constexpr uint16_t VERTICES_LIST = 0x4110;
constexpr uint16_t FACES_DESCRIPTION = 0x4120;
constexpr uint16_t MAPPING_COORDINATES = 0x4140;
constexpr uint16_t SMOOTHING_GROUPS = 0x4150;
constexpr uint16_t LOCAL_COORDINATES = 0x4160;
constexpr uint16_t FACES_MATERIAL = 0x4130;
}// namespace chunk_id

/**
 * @brief Helper struct for reading 3DS file data
 */
struct ThreeDSReader {
    luisa::vector<std::byte> data;
    size_t position = 0;

    /**
     * @brief Check if at end of data
     */
    [[nodiscard]] bool eof() const { return position >= data.size(); }

    /**
     * @brief Get remaining bytes
     */
    [[nodiscard]] size_t remaining() const { 
        return position < data.size() ? data.size() - position : 0; 
    }

    /**
     * @brief Read a uint16 in little-endian format
     */
    bool read_uint16(uint16_t &value) {
        if (position + 2 > data.size()) return false;
        auto *bytes = reinterpret_cast<const uint8_t *>(data.data() + position);
        value = static_cast<uint16_t>(bytes[0]) | 
                (static_cast<uint16_t>(bytes[1]) << 8);
        position += 2;
        return true;
    }

    /**
     * @brief Read a uint32 in little-endian format
     */
    bool read_uint32(uint32_t &value) {
        if (position + 4 > data.size()) return false;
        auto *bytes = reinterpret_cast<const uint8_t *>(data.data() + position);
        value = static_cast<uint32_t>(bytes[0]) | 
                (static_cast<uint32_t>(bytes[1]) << 8) |
                (static_cast<uint32_t>(bytes[2]) << 16) | 
                (static_cast<uint32_t>(bytes[3]) << 24);
        position += 4;
        return true;
    }

    /**
     * @brief Read a float (32-bit IEEE 754)
     */
    bool read_float(float &value) {
        if (position + 4 > data.size()) return false;
        // Read raw bytes
        uint32_t raw;
        auto *bytes = reinterpret_cast<const uint8_t *>(data.data() + position);
        raw = static_cast<uint32_t>(bytes[0]) | 
              (static_cast<uint32_t>(bytes[1]) << 8) |
              (static_cast<uint32_t>(bytes[2]) << 16) | 
              (static_cast<uint32_t>(bytes[3]) << 24);
        std::memcpy(&value, &raw, sizeof(float));
        position += 4;
        return true;
    }

    /**
     * @brief Read a single byte
     */
    bool read_byte(uint8_t &value) {
        if (position + 1 > data.size()) return false;
        value = static_cast<uint8_t>(data[position]);
        position += 1;
        return true;
    }

    /**
     * @brief Skip a number of bytes
     */
    bool skip(size_t bytes) {
        if (position + bytes > data.size()) return false;
        position += bytes;
        return true;
    }

    /**
     * @brief Skip a null-terminated string
     */
    bool skip_string() {
        while (position < data.size()) {
            uint8_t ch;
            if (!read_byte(ch)) return false;
            if (ch == 0) break;
        }
        return true;
    }

    /**
     * @brief Get current position
     */
    [[nodiscard]] size_t get_position() const { return position; }

    /**
     * @brief Set current position
     */
    void set_position(size_t pos) { position = pos; }

    /**
     * @brief Initialize from file stream
     */
    bool init_from_stream(luisa::BinaryFileStream &stream) {
        size_t len = stream.length();
        if (len == 0) return false;
        data.resize(len);
        stream.read({data.data(), data.size()});
        position = 0;
        return true;
    }
};

/**
 * @brief Parse vertices list chunk (0x4110)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_vertices_list(ThreeDSReader &reader, 
                                 MeshBuilder &mesh_builder,
                                 size_t end_pos) {
    uint16_t vertex_count;
    if (!reader.read_uint16(vertex_count)) return false;

    mesh_builder.position.reserve(vertex_count);
    for (uint16_t i = 0; i < vertex_count; ++i) {
        float x, y, z;
        if (!reader.read_float(x) || !reader.read_float(y) || 
            !reader.read_float(z)) {
            return false;
        }
        // 3DS uses right-handed coordinate system with Z up
        // Convert to standard right-handed with Y up
        mesh_builder.position.push_back(make_float3(x, z, -y));
    }

    // Ensure we're at the end position
    reader.set_position(end_pos);
    return true;
}

/**
 * @brief Parse faces description chunk (0x4120)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_faces_description(ThreeDSReader &reader,
                                     MeshBuilder &mesh_builder,
                                     size_t end_pos) {
    uint16_t face_count;
    if (!reader.read_uint16(face_count)) return false;

    auto &indices = mesh_builder.triangle_indices.emplace_back();
    indices.reserve(face_count * 3);

    for (uint16_t i = 0; i < face_count; ++i) {
        uint16_t a, b, c, flags;
        if (!reader.read_uint16(a) || !reader.read_uint16(b) || 
            !reader.read_uint16(c) || !reader.read_uint16(flags)) {
            return false;
        }
        // 3DS faces are defined clockwise, convert to counter-clockwise
        indices.push_back(a);
        indices.push_back(c);
        indices.push_back(b);
    }

    reader.set_position(end_pos);
    return true;
}

/**
 * @brief Parse mapping coordinates chunk (0x4140)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_mapping_coordinates(ThreeDSReader &reader,
                                       MeshBuilder &mesh_builder,
                                       size_t end_pos) {
    uint16_t uv_count;
    if (!reader.read_uint16(uv_count)) return false;

    auto &uvs = mesh_builder.uvs.emplace_back();
    uvs.reserve(uv_count);

    for (uint16_t i = 0; i < uv_count; ++i) {
        float u, v;
        if (!reader.read_float(u) || !reader.read_float(v)) return false;
        // 3DS V coordinates are typically flipped
        uvs.push_back(make_float2(u, 1.0f - v));
    }

    reader.set_position(end_pos);
    return true;
}

/**
 * @brief Parse triangular mesh chunk (0x4100)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_triangular_mesh(ThreeDSReader &reader,
                                   MeshBuilder &mesh_builder,
                                   size_t end_pos) {
    while (reader.get_position() < end_pos && !reader.eof()) {
        uint16_t chunk_id;
        uint32_t chunk_length;

        if (!reader.read_uint16(chunk_id)) return false;
        if (!reader.read_uint32(chunk_length)) return false;

        if (chunk_length < 6) return false;// Invalid chunk size

        size_t chunk_end = reader.get_position() + chunk_length - 6;
        if (chunk_end > end_pos) return false;

        switch (chunk_id) {
            case chunk_id::VERTICES_LIST:
                if (!parse_vertices_list(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            case chunk_id::FACES_DESCRIPTION:
                if (!parse_faces_description(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            case chunk_id::MAPPING_COORDINATES:
                if (!parse_mapping_coordinates(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            case chunk_id::SMOOTHING_GROUPS:
            case chunk_id::LOCAL_COORDINATES:
            case chunk_id::FACES_MATERIAL:
                // Skip these chunks
                reader.set_position(chunk_end);
                break;
            default:
                // Skip unknown chunks
                reader.set_position(chunk_end);
                break;
        }
    }
    return true;
}

/**
 * @brief Parse object block chunk (0x4000)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_object_block(ThreeDSReader &reader,
                                MeshBuilder &mesh_builder,
                                size_t end_pos) {
    // Skip object name (null-terminated string)
    if (!reader.skip_string()) return false;

    // Process child chunks
    while (reader.get_position() < end_pos && !reader.eof()) {
        uint16_t chunk_id;
        uint32_t chunk_length;

        if (!reader.read_uint16(chunk_id)) return false;
        if (!reader.read_uint32(chunk_length)) return false;

        if (chunk_length < 6) return false;

        size_t chunk_end = reader.get_position() + chunk_length - 6;
        if (chunk_end > end_pos) return false;

        switch (chunk_id) {
            case chunk_id::TRIANGULAR_MESH:
                if (!parse_triangular_mesh(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            default:
                // Skip other object types (camera, light, etc.)
                reader.set_position(chunk_end);
                break;
        }
    }
    return true;
}

/**
 * @brief Parse editor chunk (0x3D3D)
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 * @param end_pos End position of this chunk
 */
static bool parse_editor_chunk(ThreeDSReader &reader,
                                MeshBuilder &mesh_builder,
                                size_t end_pos) {
    while (reader.get_position() < end_pos && !reader.eof()) {
        uint16_t chunk_id;
        uint32_t chunk_length;

        if (!reader.read_uint16(chunk_id)) return false;
        if (!reader.read_uint32(chunk_length)) return false;

        if (chunk_length < 6) return false;

        size_t chunk_end = reader.get_position() + chunk_length - 6;
        if (chunk_end > end_pos) return false;

        switch (chunk_id) {
            case chunk_id::OBJECT_BLOCK:
                if (!parse_object_block(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            case chunk_id::EDITOR_VERSION:
            default:
                // Skip other editor chunks
                reader.set_position(chunk_end);
                break;
        }
    }
    return true;
}

/**
 * @brief Parse main 3DS file
 * @param reader The 3DS reader
 * @param mesh_builder The mesh builder to populate
 */
static bool parse_3ds_file(ThreeDSReader &reader, MeshBuilder &mesh_builder) {
    // Read main chunk header
    uint16_t main_id;
    uint32_t main_length;

    if (!reader.read_uint16(main_id)) return false;
    if (!reader.read_uint32(main_length)) return false;

    if (main_id != chunk_id::MAIN_CHUNK) {
        LUISA_WARNING("Invalid 3DS file: main chunk ID mismatch");
        return false;
    }

    if (main_length != reader.data.size() && main_length < 6) {
        LUISA_WARNING("Invalid 3DS file: main chunk size mismatch");
        return false;
    }

    size_t file_end = reader.get_position() + main_length - 6;
    if (file_end > reader.data.size()) {
        file_end = reader.data.size();
    }

    // Process chunks in main chunk
    while (reader.get_position() < file_end && !reader.eof()) {
        uint16_t chunk_id;
        uint32_t chunk_length;

        if (!reader.read_uint16(chunk_id)) return false;
        if (!reader.read_uint32(chunk_length)) return false;

        if (chunk_length < 6) return false;

        size_t chunk_end = reader.get_position() + chunk_length - 6;
        if (chunk_end > file_end) chunk_end = file_end;

        switch (chunk_id) {
            case chunk_id::VERSION:
                // Skip version chunk
                reader.set_position(chunk_end);
                break;
            case chunk_id::EDITOR_CHUNK:
                if (!parse_editor_chunk(reader, mesh_builder, chunk_end)) {
                    return false;
                }
                break;
            default:
                // Skip unknown chunks
                reader.set_position(chunk_end);
                break;
        }
    }

    // Validate that we have mesh data
    return !mesh_builder.position.empty() && !mesh_builder.triangle_indices.empty();
}

/**
 * @brief Calculate vertex normals from face data
 * @param mesh_builder The mesh builder containing positions and indices
 */
static void calculate_normals(MeshBuilder &mesh_builder) {
    if (mesh_builder.position.empty() || mesh_builder.triangle_indices.empty()) {
        return;
    }

    mesh_builder.normal.resize(mesh_builder.position.size(), make_float3(0.0f, 0.0f, 0.0f));

    // Accumulate face normals
    for (auto &indices : mesh_builder.triangle_indices) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if (i0 >= mesh_builder.position.size() ||
                i1 >= mesh_builder.position.size() ||
                i2 >= mesh_builder.position.size()) {
                continue;
            }

            float3 p0 = mesh_builder.position[i0];
            float3 p1 = mesh_builder.position[i1];
            float3 p2 = mesh_builder.position[i2];

            float3 edge1 = p1 - p0;
            float3 edge2 = p2 - p0;
            float3 normal = cross(edge1, edge2);

            // Accumulate without normalizing yet
            mesh_builder.normal[i0] += normal;
            mesh_builder.normal[i1] += normal;
            mesh_builder.normal[i2] += normal;
        }
    }

    // Normalize all normals
    for (auto &n : mesh_builder.normal) {
        float len_sq = dot(n, n);
        if (len_sq > 0.0f) {
            n = n / std::sqrt(len_sq);
        } else {
            n = make_float3(0.0f, 1.0f, 0.0f);// Default normal
        }
    }
}

bool ThreeDSMeshImporter::import(Resource *resource_base,
                                  luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) {
        LUISA_WARNING("Failed to open 3DS file: {}", path.string());
        return false;
    }

    ThreeDSReader reader;
    if (!reader.init_from_stream(file_stream)) {
        LUISA_WARNING("Failed to read 3DS file: {}", path.string());
        return false;
    }

    MeshBuilder mesh_builder;

    if (!parse_3ds_file(reader, mesh_builder)) {
        LUISA_WARNING("Failed to parse 3DS file: {}", path.string());
        return false;
    }

    if (mesh_builder.position.empty()) {
        LUISA_WARNING("No vertices found in 3DS file: {}", path.string());
        return false;
    }

    if (mesh_builder.triangle_indices.empty()) {
        LUISA_WARNING("No faces found in 3DS file: {}", path.string());
        return false;
    }

    // Calculate normals if not present
    if (mesh_builder.normal.empty()) {
        calculate_normals(mesh_builder);
    }

    // Calculate tangents if UVs are present
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
                vstd::push_back_all(triangles,
                                    luisa::span{(Triangle *)i.data(), 
                                                i.size() / 3});
            }
            calculate_tangent(mesh_builder.position, mesh_builder.uvs[0],
                              mesh_builder.tangent, triangles, 1.0f);
        } else if (!mesh_builder.triangle_indices.empty()) {
            calculate_tangent(
                mesh_builder.position,
                mesh_builder.uvs[0],
                mesh_builder.tangent,
                luisa::span{
                    (Triangle const *)(mesh_builder.triangle_indices[0].data()),
                    mesh_builder.triangle_indices[0].size() / 3},
                1.0f);
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


    LUISA_INFO("Successfully imported 3DS mesh: {} vertices, {} triangles",
               mesh_builder.vertex_count(),
               mesh_builder.indices_count() / 3);

    return true;
}

}// namespace rbc::world
