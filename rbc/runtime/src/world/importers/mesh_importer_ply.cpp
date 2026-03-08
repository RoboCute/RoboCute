#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_ply.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <cstring>
#include <cctype>
#include <string>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

namespace {

/**
 * @brief Property types supported in PLY format
 */
enum class PlyPropertyType {
    Invalid,
    Char, UChar,
    Short, UShort,
    Int, UInt,
    Float, Double
};

/**
 * @brief Parse property type from string
 */
PlyPropertyType parse_property_type(string_view str) {
    if (str == "char" || str == "int8") return PlyPropertyType::Char;
    if (str == "uchar" || str == "uint8") return PlyPropertyType::UChar;
    if (str == "short" || str == "int16") return PlyPropertyType::Short;
    if (str == "ushort" || str == "uint16") return PlyPropertyType::UShort;
    if (str == "int" || str == "int32") return PlyPropertyType::Int;
    if (str == "uint" || str == "uint32") return PlyPropertyType::UInt;
    if (str == "float" || str == "float32") return PlyPropertyType::Float;
    if (str == "double" || str == "float64") return PlyPropertyType::Double;
    return PlyPropertyType::Invalid;
}

/**
 * @brief Get size of property type in bytes
 */
size_t property_type_size(PlyPropertyType type) {
    switch (type) {
        case PlyPropertyType::Char:
        case PlyPropertyType::UChar: return 1;
        case PlyPropertyType::Short:
        case PlyPropertyType::UShort: return 2;
        case PlyPropertyType::Int:
        case PlyPropertyType::UInt:
        case PlyPropertyType::Float: return 4;
        case PlyPropertyType::Double: return 8;
        default: return 0;
    }
}

/**
 * @brief Read a value from binary buffer based on type
 */
template<typename T>
T read_value(const std::byte *&ptr, PlyPropertyType type, bool swap_endian) {
    T result = T{};
    size_t size = property_type_size(type);
    
    switch (type) {
        case PlyPropertyType::Char: {
            int8_t v;
            std::memcpy(&v, ptr, 1);
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::UChar: {
            uint8_t v;
            std::memcpy(&v, ptr, 1);
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::Short: {
            int16_t v;
            std::memcpy(&v, ptr, 2);
            if (swap_endian) {
                v = static_cast<int16_t>((v >> 8) | (v << 8));
            }
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::UShort: {
            uint16_t v;
            std::memcpy(&v, ptr, 2);
            if (swap_endian) {
                v = static_cast<uint16_t>((v >> 8) | (v << 8));
            }
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::Int: {
            int32_t v;
            std::memcpy(&v, ptr, 4);
            if (swap_endian) {
                v = (v >> 24) | ((v >> 8) & 0xFF00) | 
                    ((v << 8) & 0xFF0000) | (v << 24);
            }
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::UInt: {
            uint32_t v;
            std::memcpy(&v, ptr, 4);
            if (swap_endian) {
                v = (v >> 24) | ((v >> 8) & 0xFF00) | 
                    ((v << 8) & 0xFF0000) | (v << 24);
            }
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::Float: {
            float v;
            std::memcpy(&v, ptr, 4);
            if (swap_endian) {
                uint32_t u;
                std::memcpy(&u, &v, 4);
                u = (u >> 24) | ((u >> 8) & 0xFF00) | 
                    ((u << 8) & 0xFF0000) | (u << 24);
                std::memcpy(&v, &u, 4);
            }
            result = static_cast<T>(v);
            break;
        }
        case PlyPropertyType::Double: {
            double v;
            std::memcpy(&v, ptr, 8);
            if (swap_endian) {
                uint64_t u;
                std::memcpy(&u, &v, 8);
                u = (u >> 56) | ((u >> 40) & 0xFF00) | ((u >> 24) & 0xFF0000) |
                    ((u >> 8) & 0xFF000000) | ((u << 8) & 0xFF00000000ULL) |
                    ((u << 24) & 0xFF0000000000ULL) | ((u << 40) & 0xFF000000000000ULL) |
                    (u << 56);
                std::memcpy(&v, &u, 8);
            }
            result = static_cast<T>(v);
            break;
        }
        default: break;
    }
    ptr += size;
    return result;
}

/**
 * @brief Represents a single property in PLY element
 */
struct PlyProperty {
    luisa::string name;
    PlyPropertyType type;
    bool is_list;
    PlyPropertyType list_count_type;
    PlyPropertyType list_item_type;
    
    PlyProperty() : type(PlyPropertyType::Invalid), is_list(false),
                    list_count_type(PlyPropertyType::Invalid), 
                    list_item_type(PlyPropertyType::Invalid) {}
};

/**
 * @brief Represents an element (vertex/face/etc.) in PLY
 */
struct PlyElement {
    luisa::string name;
    size_t count;
    luisa::vector<PlyProperty> properties;
    
    PlyElement() : count(0) {}
};

/**
 * @brief PLY file format type
 */
enum class PlyFormat {
    Invalid,
    ASCII,
    BinaryLittleEndian,
    BinaryBigEndian
};

/**
 * @brief Read a line from buffer
 */
const char *read_line(const char *ptr, const char *end, luisa::string &line) {
    line.clear();
    while (ptr < end && *ptr != '\n' && *ptr != '\r') {
        line.push_back(*ptr++);
    }
    // Skip newline
    if (ptr < end && *ptr == '\r') ptr++;
    if (ptr < end && *ptr == '\n') ptr++;
    return ptr;
}

/**
 * @brief Trim whitespace from string
 */
luisa::string_view trim(luisa::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        start++;
    }
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
        end--;
    }
    return sv.substr(start, end - start);
}

/**
 * @brief Split string by whitespace
 */
luisa::vector<luisa::string_view> split(luisa::string_view sv) {
    luisa::vector<luisa::string_view> parts;
    size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) i++;
        if (i >= sv.size()) break;
        size_t start = i;
        while (i < sv.size() && !std::isspace(static_cast<unsigned char>(sv[i]))) i++;
        parts.push_back(sv.substr(start, i - start));
    }
    return parts;
}

/**
 * @brief Parse PLY header and return data offset
 */
bool parse_ply_header(const char *data, size_t size, PlyFormat &format,
                      luisa::vector<PlyElement> &elements, const char *&data_start) {
    luisa::string line;
    const char *ptr = data;
    const char *end = data + size;
    
    // First line must be "ply"
    ptr = read_line(ptr, end, line);
    if (trim(line) != "ply") {
        return false;
    }
    
    format = PlyFormat::Invalid;
    PlyElement *current_element = nullptr;
    
    while (ptr < end) {
        ptr = read_line(ptr, end, line);
        luisa::string_view trimmed = trim(line);
        
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;  // Skip empty lines and comments
        }
        
        auto parts = split(trimmed);
        if (parts.empty()) continue;
        
        if (parts[0] == "format") {
            if (parts.size() < 3) return false;
            if (parts[1] == "ascii") {
                format = PlyFormat::ASCII;
            } else if (parts[1] == "binary_little_endian") {
                format = PlyFormat::BinaryLittleEndian;
            } else if (parts[1] == "binary_big_endian") {
                format = PlyFormat::BinaryBigEndian;
            }
        } else if (parts[0] == "element") {
            if (parts.size() < 3) return false;
            auto &elem = elements.emplace_back();
            elem.name = luisa::string(parts[1]);
            elem.count = static_cast<size_t>(std::stoull(std::string(parts[2])));
            current_element = &elem;
        } else if (parts[0] == "property") {
            if (!current_element) return false;
            if (parts.size() < 3) return false;
            
            auto &prop = current_element->properties.emplace_back();
            
            if (parts[1] == "list") {
                // List property: property list <count_type> <item_type> <name>
                if (parts.size() < 5) return false;
                prop.is_list = true;
                prop.list_count_type = parse_property_type(parts[2]);
                prop.list_item_type = parse_property_type(parts[3]);
                prop.name = luisa::string(parts[4]);
            } else {
                // Scalar property: property <type> <name>
                prop.type = parse_property_type(parts[1]);
                prop.name = luisa::string(parts[2]);
            }
        } else if (parts[0] == "end_header") {
            data_start = ptr;
            return format != PlyFormat::Invalid;
        }
    }
    
    return false;  // Missing end_header
}

/**
 * @brief Parse ASCII vertex data
 */
void parse_ascii_vertices(const char *&ptr, const char *end, const PlyElement &element,
                          MeshBuilder &mesh_builder) {
    luisa::string line;
    
    // Find property indices
    int x_idx = -1, y_idx = -1, z_idx = -1;
    int nx_idx = -1, ny_idx = -1, nz_idx = -1;
    int s_idx = -1, t_idx = -1;
    
    for (size_t i = 0; i < element.properties.size(); i++) {
        const auto &prop = element.properties[i];
        if (prop.name == "x") x_idx = static_cast<int>(i);
        else if (prop.name == "y") y_idx = static_cast<int>(i);
        else if (prop.name == "z") z_idx = static_cast<int>(i);
        else if (prop.name == "nx") nx_idx = static_cast<int>(i);
        else if (prop.name == "ny") ny_idx = static_cast<int>(i);
        else if (prop.name == "nz") nz_idx = static_cast<int>(i);
        else if (prop.name == "s" || prop.name == "u" || prop.name == "texture_u") {
            s_idx = static_cast<int>(i);
        } else if (prop.name == "t" || prop.name == "v" || prop.name == "texture_v") {
            t_idx = static_cast<int>(i);
        }
    }
    
    bool has_normal = nx_idx >= 0 && ny_idx >= 0 && nz_idx >= 0;
    bool has_uv = s_idx >= 0 && t_idx >= 0;
    
    mesh_builder.position.reserve(mesh_builder.position.size() + element.count);
    if (has_normal) {
        mesh_builder.normal.reserve(mesh_builder.normal.size() + element.count);
    }
    if (has_uv) {
        if (mesh_builder.uvs.empty()) {
            mesh_builder.uvs.emplace_back();
        }
        mesh_builder.uvs[0].reserve(mesh_builder.uvs[0].size() + element.count);
    }
    
    for (size_t v = 0; v < element.count && ptr < end; v++) {
        ptr = read_line(ptr, end, line);
        auto parts = split(trim(line));
        
        // Position (required)
        float x = (x_idx >= 0 && x_idx < parts.size()) ? 
                  static_cast<float>(std::stod(std::string(parts[x_idx]))) : 0.0f;
        float y = (y_idx >= 0 && y_idx < parts.size()) ? 
                  static_cast<float>(std::stod(std::string(parts[y_idx]))) : 0.0f;
        float z = (z_idx >= 0 && z_idx < parts.size()) ? 
                  static_cast<float>(std::stod(std::string(parts[z_idx]))) : 0.0f;
        mesh_builder.position.push_back(make_float3(x, y, z));
        
        // Normal
        if (has_normal) {
            float nx = (nx_idx >= 0 && nx_idx < parts.size()) ? 
                       static_cast<float>(std::stod(std::string(parts[nx_idx]))) : 0.0f;
            float ny = (ny_idx >= 0 && ny_idx < parts.size()) ? 
                       static_cast<float>(std::stod(std::string(parts[ny_idx]))) : 0.0f;
            float nz = (nz_idx >= 0 && nz_idx < parts.size()) ? 
                       static_cast<float>(std::stod(std::string(parts[nz_idx]))) : 0.0f;
            mesh_builder.normal.push_back(make_float3(nx, ny, nz));
        }
        
        // UV
        if (has_uv) {
            float u = (s_idx >= 0 && s_idx < parts.size()) ? 
                      static_cast<float>(std::stod(std::string(parts[s_idx]))) : 0.0f;
            float v_coord = (t_idx >= 0 && t_idx < parts.size()) ? 
                            static_cast<float>(std::stod(std::string(parts[t_idx]))) : 0.0f;
            mesh_builder.uvs[0].push_back(make_float2(u, v_coord));
        }
    }
}

/**
 * @brief Parse binary vertex data
 */
void parse_binary_vertices(const std::byte *&ptr, const PlyElement &element,
                           MeshBuilder &mesh_builder, bool swap_endian) {
    // Find property indices
    int x_idx = -1, y_idx = -1, z_idx = -1;
    int nx_idx = -1, ny_idx = -1, nz_idx = -1;
    int s_idx = -1, t_idx = -1;
    
    for (size_t i = 0; i < element.properties.size(); i++) {
        const auto &prop = element.properties[i];
        if (prop.name == "x") x_idx = static_cast<int>(i);
        else if (prop.name == "y") y_idx = static_cast<int>(i);
        else if (prop.name == "z") z_idx = static_cast<int>(i);
        else if (prop.name == "nx") nx_idx = static_cast<int>(i);
        else if (prop.name == "ny") ny_idx = static_cast<int>(i);
        else if (prop.name == "nz") nz_idx = static_cast<int>(i);
        else if (prop.name == "s" || prop.name == "u" || prop.name == "texture_u") {
            s_idx = static_cast<int>(i);
        } else if (prop.name == "t" || prop.name == "v" || prop.name == "texture_v") {
            t_idx = static_cast<int>(i);
        }
    }
    
    bool has_normal = nx_idx >= 0 && ny_idx >= 0 && nz_idx >= 0;
    bool has_uv = s_idx >= 0 && t_idx >= 0;
    
    mesh_builder.position.reserve(mesh_builder.position.size() + element.count);
    if (has_normal) {
        mesh_builder.normal.reserve(mesh_builder.normal.size() + element.count);
    }
    if (has_uv) {
        if (mesh_builder.uvs.empty()) {
            mesh_builder.uvs.emplace_back();
        }
        mesh_builder.uvs[0].reserve(mesh_builder.uvs[0].size() + element.count);
    }
    
    for (size_t v = 0; v < element.count; v++) {
        luisa::vector<double> values;
        values.reserve(element.properties.size());
        
        // Read all properties for this vertex
        for (const auto &prop : element.properties) {
            if (prop.is_list) {
                // Skip list properties for vertices (shouldn't normally occur)
                uint32_t count = read_value<uint32_t>(ptr, prop.list_count_type, swap_endian);
                for (uint32_t i = 0; i < count; i++) {
                    read_value<double>(ptr, prop.list_item_type, swap_endian);
                }
                values.push_back(0);
            } else {
                values.push_back(read_value<double>(ptr, prop.type, swap_endian));
            }
        }
        
        // Position (required)
        float x = (x_idx >= 0) ? static_cast<float>(values[x_idx]) : 0.0f;
        float y = (y_idx >= 0) ? static_cast<float>(values[y_idx]) : 0.0f;
        float z = (z_idx >= 0) ? static_cast<float>(values[z_idx]) : 0.0f;
        mesh_builder.position.push_back(make_float3(x, y, z));
        
        // Normal
        if (has_normal) {
            float nx = (nx_idx >= 0) ? static_cast<float>(values[nx_idx]) : 0.0f;
            float ny = (ny_idx >= 0) ? static_cast<float>(values[ny_idx]) : 0.0f;
            float nz = (nz_idx >= 0) ? static_cast<float>(values[nz_idx]) : 0.0f;
            mesh_builder.normal.push_back(make_float3(nx, ny, nz));
        }
        
        // UV
        if (has_uv) {
            float u = (s_idx >= 0) ? static_cast<float>(values[s_idx]) : 0.0f;
            float v_coord = (t_idx >= 0) ? static_cast<float>(values[t_idx]) : 0.0f;
            mesh_builder.uvs[0].push_back(make_float2(u, v_coord));
        }
    }
}

/**
 * @brief Parse ASCII face data
 */
void parse_ascii_faces(const char *&ptr, const char *end, const PlyElement &element,
                       MeshBuilder &mesh_builder) {
    luisa::string line;
    
    // Find vertex_indices property
    int indices_idx = -1;
    for (size_t i = 0; i < element.properties.size(); i++) {
        const auto &prop = element.properties[i];
        if (prop.is_list && (prop.name == "vertex_indices" || 
                             prop.name == "vertex_index" ||
                             prop.name == "indices")) {
            indices_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (indices_idx < 0) return;  // No face indices found
    
    auto &indices = mesh_builder.triangle_indices.emplace_back();
    size_t estimated_triangles = element.count;  // At least this many, likely more
    indices.reserve(estimated_triangles * 3);
    
    for (size_t f = 0; f < element.count && ptr < end; f++) {
        ptr = read_line(ptr, end, line);
        auto parts = split(trim(line));
        
        if (parts.empty()) continue;
        
        // First value is the count
        int count = std::stoi(std::string(parts[0]));
        if (count < 3) continue;  // Need at least a triangle
        
        // Read vertex indices
        luisa::vector<uint> face_indices;
        face_indices.reserve(count);
        for (int i = 1; i <= count && i < parts.size(); i++) {
            face_indices.push_back(static_cast<uint>(std::stoi(std::string(parts[i]))));
        }
        
        // Triangulate (simple fan triangulation)
        for (int i = 2; i < face_indices.size(); i++) {
            indices.push_back(face_indices[0]);
            indices.push_back(face_indices[i - 1]);
            indices.push_back(face_indices[i]);
        }
    }
}

/**
 * @brief Parse binary face data
 */
void parse_binary_faces(const std::byte *&ptr, const PlyElement &element,
                        MeshBuilder &mesh_builder, bool swap_endian) {
    // Find vertex_indices property
    int indices_idx = -1;
    const PlyProperty *indices_prop = nullptr;
    for (size_t i = 0; i < element.properties.size(); i++) {
        const auto &prop = element.properties[i];
        if (prop.is_list && (prop.name == "vertex_indices" || 
                             prop.name == "vertex_index" ||
                             prop.name == "indices")) {
            indices_idx = static_cast<int>(i);
            indices_prop = &prop;
            break;
        }
    }
    
    if (!indices_prop || indices_idx < 0) {
        // Skip all face data if we can't find indices
        for (size_t f = 0; f < element.count; f++) {
            for (const auto &prop : element.properties) {
                if (prop.is_list) {
                    uint32_t count = read_value<uint32_t>(ptr, prop.list_count_type, swap_endian);
                    for (uint32_t i = 0; i < count; i++) {
                        read_value<double>(ptr, prop.list_item_type, swap_endian);
                    }
                } else {
                    read_value<double>(ptr, prop.type, swap_endian);
                }
            }
        }
        return;
    }
    
    auto &indices = mesh_builder.triangle_indices.emplace_back();
    size_t estimated_triangles = element.count;
    indices.reserve(estimated_triangles * 3);
    
    for (size_t f = 0; f < element.count; f++) {
        luisa::vector<uint> face_indices;
        
        for (const auto &prop : element.properties) {
            if (prop.is_list) {
                uint32_t count = read_value<uint32_t>(ptr, prop.list_count_type, swap_endian);
                if (&prop == indices_prop) {
                    face_indices.reserve(count);
                    for (uint32_t i = 0; i < count; i++) {
                        face_indices.push_back(read_value<uint>(ptr, prop.list_item_type, swap_endian));
                    }
                } else {
                    // Skip other list properties
                    for (uint32_t i = 0; i < count; i++) {
                        read_value<double>(ptr, prop.list_item_type, swap_endian);
                    }
                }
            } else {
                read_value<double>(ptr, prop.type, swap_endian);
            }
        }
        
        // Triangulate (simple fan triangulation)
        if (face_indices.size() >= 3) {
            for (size_t i = 2; i < face_indices.size(); i++) {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[i - 1]);
                indices.push_back(face_indices[i]);
            }
        }
    }
}

}// namespace

bool PlyMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }
    
    // Read file
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return false;
    
    luisa::string file_data;
    file_data.resize(file_stream.length());
    file_stream.read({(std::byte *)file_data.data(), file_data.size()});
    
    // Parse header
    PlyFormat format;
    luisa::vector<PlyElement> elements;
    const char *data_start = nullptr;
    
    if (!parse_ply_header(file_data.data(), file_data.size(), format, elements, data_start)) {
        LUISA_WARNING("Failed to parse PLY header");
        return false;
    }
    
    bool swap_endian = (format == PlyFormat::BinaryBigEndian);
    MeshBuilder mesh_builder;
    
    // Parse elements
    if (format == PlyFormat::ASCII) {
        const char *ptr = data_start;
        const char *end = file_data.data() + file_data.size();
        
        for (const auto &element : elements) {
            if (element.name == "vertex") {
                parse_ascii_vertices(ptr, end, element, mesh_builder);
            } else if (element.name == "face") {
                parse_ascii_faces(ptr, end, element, mesh_builder);
            } else {
                // Skip unknown elements
                luisa::string line;
                for (size_t i = 0; i < element.count && ptr < end; i++) {
                    ptr = read_line(ptr, end, line);
                }
            }
        }
    } else {
        // Binary format
        const std::byte *ptr = reinterpret_cast<const std::byte *>(data_start);
        
        for (const auto &element : elements) {
            if (element.name == "vertex") {
                parse_binary_vertices(ptr, element, mesh_builder, swap_endian);
            } else if (element.name == "face") {
                parse_binary_faces(ptr, element, mesh_builder, swap_endian);
            } else {
                // Skip unknown elements
                for (size_t i = 0; i < element.count; i++) {
                    for (const auto &prop : element.properties) {
                        if (prop.is_list) {
                            uint32_t count = read_value<uint32_t>(ptr, prop.list_count_type, swap_endian);
                            for (uint32_t j = 0; j < count; j++) {
                                read_value<double>(ptr, prop.list_item_type, swap_endian);
                            }
                        } else {
                            read_value<double>(ptr, prop.type, swap_endian);
                        }
                    }
                }
            }
        }
    }
    
    // Validate mesh data
    if (mesh_builder.position.empty()) {
        LUISA_WARNING("PLY file contains no vertices");
        return false;
    }
    
    // If no faces, create trivial indices (point cloud mode - not fully supported)
    if (mesh_builder.triangle_indices.empty()) {
        LUISA_WARNING("PLY file contains no faces");
        return false;
    }
    
    // Calculate tangents if we have both UVs and normals
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
    
    // Write to resource
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(std::move(submesh_offsets), mesh_builder.vertex_count(), 
                           mesh_builder.indices_count() / 3, mesh_builder.uv_count(), 
                           mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);
    
    return true;
}

}// namespace rbc::world
