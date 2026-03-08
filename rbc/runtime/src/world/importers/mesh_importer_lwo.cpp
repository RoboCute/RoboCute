#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_lwo.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <cstring>
#include <fstream>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

namespace {

// LWO chunk IDs (big-endian fourCC)
constexpr uint32_t make_id(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) |
           static_cast<uint32_t>(d);
}

constexpr uint32_t ID_FORM = make_id('F', 'O', 'R', 'M');
constexpr uint32_t ID_LWO2 = make_id('L', 'W', 'O', '2');
constexpr uint32_t ID_LAYR = make_id('L', 'A', 'Y', 'R');
constexpr uint32_t ID_PNTS = make_id('P', 'N', 'T', 'S');
constexpr uint32_t ID_POLS = make_id('P', 'O', 'L', 'S');
constexpr uint32_t ID_VMAP = make_id('V', 'M', 'A', 'P');
constexpr uint32_t ID_VMAD = make_id('V', 'M', 'A', 'D');
constexpr uint32_t ID_TAGS = make_id('T', 'A', 'G', 'S');
constexpr uint32_t ID_PTAG = make_id('P', 'T', 'A', 'G');
constexpr uint32_t ID_SURF = make_id('S', 'U', 'R', 'F');

// VMAP/VMAD types
constexpr uint32_t ID_TXUV = make_id('T', 'X', 'U', 'V');
constexpr uint32_t ID_RGB = make_id('R', 'G', 'B', ' ');
constexpr uint32_t ID_RGBA = make_id('R', 'G', 'B', 'A');
constexpr uint32_t ID_NORM = make_id('N', 'O', 'R', 'M');
constexpr uint32_t ID_WGHT = make_id('W', 'G', 'H', 'T');

// PTAG types
constexpr uint32_t ID_SURF_ID = make_id('S', 'U', 'R', 'F');
constexpr uint32_t ID_PART = make_id('P', 'A', 'R', 'T');
constexpr uint32_t ID_SMGP = make_id('S', 'M', 'G', 'P');

/**
 * @brief Binary reader for LWO file format (big-endian)
 */
class LwoReader {
public:
    explicit LwoReader(luisa::BinaryFileStream &stream) : _stream(stream) {
        _data.resize(stream.length());
        _stream.read({reinterpret_cast<std::byte *>(_data.data()), _data.size()});
        _pos = 0;
    }

    [[nodiscard]] bool valid() const { return !_data.empty(); }

    [[nodiscard]] size_t pos() const { return _pos; }

    [[nodiscard]] size_t size() const { return _data.size(); }

    [[nodiscard]] bool eof() const { return _pos >= _data.size(); }

    void seek(size_t pos) { _pos = std::min(pos, _data.size()); }

    template<typename T>
    T read() {
        T value;
        if (_pos + sizeof(T) <= _data.size()) {
            std::memcpy(&value, _data.data() + _pos, sizeof(T));
            _pos += sizeof(T);
        } else {
            std::memset(&value, 0, sizeof(T));
        }
        return value;
    }

    [[nodiscard]] uint8_t read_u8() { return read<uint8_t>(); }

    [[nodiscard]] uint16_t read_u16() {
        uint16_t value = read<uint16_t>();
        // Big-endian to host
        return (value >> 8) | (value << 8);
    }

    [[nodiscard]] uint32_t read_u32() {
        uint32_t value = read<uint32_t>();
        // Big-endian to host
        return ((value >> 24) & 0xFF) |
               ((value >> 8) & 0xFF00) |
               ((value << 8) & 0xFF0000) |
               ((value << 24) & 0xFF000000);
    }

    [[nodiscard]] float read_f32() {
        uint32_t iv = read<uint32_t>();
        // Big-endian to host
        iv = ((iv >> 24) & 0xFF) |
             ((iv >> 8) & 0xFF00) |
             ((iv << 8) & 0xFF0000) |
             ((iv << 24) & 0xFF000000);
        float value;
        std::memcpy(&value, &iv, sizeof(float));
        return value;
    }

    [[nodiscard]] int16_t read_s16() {
        return static_cast<int16_t>(read_u16());
    }

    [[nodiscard]] int32_t read_s32() {
        return static_cast<int32_t>(read_u32());
    }

    [[nodiscard]] luisa::string read_string() {
        luisa::string result;
        while (_pos < _data.size()) {
            char c = static_cast<char>(_data[_pos++]);
            if (c == '\0') break;
            result.push_back(c);
        }
        // Strings are padded to even length
        if ((result.size() + 1) % 2 != 0 && _pos < _data.size()) {
            _pos++;
        }
        return result;
    }

    void skip(size_t bytes) {
        _pos = std::min(_pos + bytes, _data.size());
    }

    [[nodiscard]] luisa::string_view data_at(size_t pos, size_t len) const {
        if (pos + len <= _data.size()) {
            return luisa::string_view(_data.data() + pos, len);
        }
        return {};
    }

private:
    luisa::BinaryFileStream &_stream;
    luisa::vector<char> _data;
    size_t _pos;
};

/**
 * @brief Vertex index with displacement (for LWO polygon format)
 */
struct LwoVertexRef {
    uint32_t index;
    bool is_new;
};

/**
 * @brief Parse variable-length vertex index from LWO POLS chunk
 * @return Vertex index and whether it's a new polygon start
 */
[[nodiscard]] LwoVertexRef read_vertex_index(LwoReader &reader) {
    uint32_t value = reader.read_u16();
    if (value == 0xFF00) {
        // New polygon start marker (32-bit index follows)
        value = reader.read_u32();
        return {value, true};
    } else if (value & 0x8000) {
        // Negative value (32-bit index)
        value = ((value & 0x7FFF) << 16) | reader.read_u16();
        return {value, true};
    }
    return {value, false};
}

}// namespace

bool LwoMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) {
        LUISA_WARNING("Failed to open LWO file: {}", path.string());
        return false;
    }

    LwoReader reader(file_stream);
    if (!reader.valid()) {
        LUISA_WARNING("Failed to read LWO file: {}", path.string());
        return false;
    }

    // Read FORM header
    uint32_t form_id = reader.read_u32();
    if (form_id != ID_FORM) {
        LUISA_WARNING("Invalid LWO file: missing FORM header");
        return false;
    }

    uint32_t form_size = reader.read_u32();
    uint32_t file_type = reader.read_u32();

    if (file_type != ID_LWO2) {
        LUISA_WARNING("Unsupported LWO format (only LWO2 supported)");
        return false;
    }

    MeshBuilder mesh_builder;
    luisa::vector<float3> temp_normals;
    luisa::vector<float2> temp_uvs;
    luisa::vector<uint32_t> normal_indices;
    luisa::vector<uint32_t> uv_indices;
    bool has_normals = false;
    bool has_uvs = false;
    luisa::vector<luisa::vector<uint32_t>> polygon_vertices;

    // Parse chunks
    while (!reader.eof()) {
        if (reader.pos() + 8 > reader.size()) break;

        uint32_t chunk_id = reader.read_u32();
        uint32_t chunk_size = reader.read_u32();
        size_t chunk_start = reader.pos();

        switch (chunk_id) {
            case ID_PNTS: {
                // Points (vertices)
                uint32_t num_points = chunk_size / 12;
                mesh_builder.position.reserve(num_points);
                for (uint32_t i = 0; i < num_points; i++) {
                    float x = reader.read_f32();
                    float y = reader.read_f32();
                    float z = reader.read_f32();
                    mesh_builder.position.push_back(make_float3(x, y, z));
                }
                break;
            }

            case ID_POLS: {
                // Polygons
                // Read polygon type
                uint32_t poly_type = reader.read_u32();
                size_t data_start = reader.pos();
                size_t data_size = chunk_size - 4;

                // Parse polygons
                while (reader.pos() < data_start + data_size) {
                    luisa::vector<uint32_t> poly;

                    // Read number of vertices (variable length)
                    uint32_t num_verts = reader.read_u16();
                    if (num_verts == 0xFF00) {
                        num_verts = reader.read_u32();
                    } else if (num_verts & 0x8000) {
                        num_verts = ((num_verts & 0x7FFF) << 16) | reader.read_u16();
                    }

                    // Read vertex indices
                    poly.reserve(num_verts);
                    for (uint32_t i = 0; i < num_verts; i++) {
                        LwoVertexRef ref = read_vertex_index(reader);
                        poly.push_back(ref.index);
                    }

                    if (!poly.empty()) {
                        polygon_vertices.push_back(std::move(poly));
                    }
                }
                break;
            }

            case ID_VMAP:
            case ID_VMAD: {
                // Vertex maps (UVs, normals, etc.)
                uint32_t vmap_type = reader.read_u32();
                uint16_t dimension = reader.read_u16();
                luisa::string name = reader.read_string();

                if (vmap_type == ID_TXUV && dimension == 2) {
                    // UV map
                    has_uvs = true;
                    size_t num_entries = (chunk_size - 4 - 2 - name.size() -
                                          (name.size() % 2 == 0 ? 2 : 1)) /
                                         (4 + dimension * 4);

                    temp_uvs.reserve(num_entries);
                    uv_indices.reserve(num_entries);

                    size_t entry_data_size = chunk_start + chunk_size - reader.pos();
                    while (entry_data_size >= 4 + dimension * 4) {
                        uint32_t vert_index = reader.read_u32();
                        float u = reader.read_f32();
                        float v = reader.read_f32();
                        temp_uvs.push_back(make_float2(u, v));
                        uv_indices.push_back(vert_index);
                        entry_data_size -= (4 + dimension * 4);
                    }
                } else if ((vmap_type == ID_NORM || vmap_type == ID_RGB) && dimension == 3) {
                    // Normal map
                    has_normals = true;
                    size_t num_entries = (chunk_size - 4 - 2 - name.size() -
                                          (name.size() % 2 == 0 ? 2 : 1)) /
                                         (4 + dimension * 4);

                    temp_normals.reserve(num_entries);
                    normal_indices.reserve(num_entries);

                    size_t entry_data_size = chunk_start + chunk_size - reader.pos();
                    while (entry_data_size >= 4 + dimension * 4) {
                        uint32_t vert_index = reader.read_u32();
                        float nx = reader.read_f32();
                        float ny = reader.read_f32();
                        float nz = reader.read_f32();
                        temp_normals.push_back(make_float3(nx, ny, nz));
                        normal_indices.push_back(vert_index);
                        entry_data_size -= (4 + dimension * 4);
                    }
                }
                break;
            }

            case ID_LAYR:
            case ID_TAGS:
            case ID_PTAG:
            case ID_SURF:
            default: {
                // Skip unknown chunks
                break;
            }
        }

        // Seek to end of chunk (handle padding)
        size_t chunk_end = chunk_start + chunk_size;
        if (chunk_size % 2 != 0) chunk_end++;
        reader.seek(chunk_end);
    }

    // Process polygons into triangles
    if (!polygon_vertices.empty()) {
        auto &indices = mesh_builder.triangle_indices.emplace_back();

        for (auto &poly : polygon_vertices) {
            if (poly.size() < 3) continue;

            // Triangulate polygon (fan triangulation)
            for (size_t i = 1; i + 1 < poly.size(); i++) {
                indices.push_back(poly[0]);
                indices.push_back(poly[i]);
                indices.push_back(poly[i + 1]);
            }
        }
    }

    // Apply vertex normals if available
    if (has_normals && !temp_normals.empty() && !normal_indices.empty()) {
        mesh_builder.normal.resize(mesh_builder.position.size());
        for (size_t i = 0; i < normal_indices.size() && i < temp_normals.size(); i++) {
            uint32_t idx = normal_indices[i];
            if (idx < mesh_builder.normal.size()) {
                mesh_builder.normal[idx] = temp_normals[i];
            }
        }
    }

    // Apply UVs if available
    if (has_uvs && !temp_uvs.empty() && !uv_indices.empty()) {
        auto &uvs = mesh_builder.uvs.emplace_back();
        uvs.resize(mesh_builder.position.size());
        for (size_t i = 0; i < uv_indices.size() && i < temp_uvs.size(); i++) {
            uint32_t idx = uv_indices[i];
            if (idx < uvs.size()) {
                uvs[idx] = temp_uvs[i];
            }
        }
    }

    // Calculate tangents if we have UVs and positions
    if (mesh_builder.uv_count() > 0 && mesh_builder.position.size() > 0) {
        mesh_builder.tangent.push_back_uninitialized(mesh_builder.vertex_count());
        if (!mesh_builder.triangle_indices.empty() && !mesh_builder.triangle_indices[0].empty()) {
            calculate_tangent(
                mesh_builder.position,
                mesh_builder.uvs[0],
                mesh_builder.tangent,
                luisa::span{
                    reinterpret_cast<Triangle const *>(mesh_builder.triangle_indices[0].data()),
                    mesh_builder.triangle_indices[0].size() / 3},
                1);
        }
    }

    // Validate mesh data
    if (mesh_builder.position.empty() || mesh_builder.triangle_indices.empty() ||
        mesh_builder.triangle_indices[0].empty()) {
        LUISA_WARNING("LWO mesh has no valid geometry");
        return false;
    }

    // Write to resource
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
