#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_off.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <fstream>
#include <sstream>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool OffMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    // Open file
    std::ifstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING_WITH_LOCATION("Failed to open OFF file: {}", path.string());
        return false;
    }

    // Read and parse the file
    std::string line;
    
    // Skip empty lines and comments at the beginning
    while (std::getline(file, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        // Skip comment lines
        if (line[0] == '#') continue;
        break;
    }
    
    // Check magic string
    if (line.substr(0, 3) != "OFF") {
        LUISA_WARNING_WITH_LOCATION("Invalid OFF file format: missing 'OFF' header");
        return false;
    }
    
    // The rest of the first line or next lines may contain counts
    // Handle cases where OFF is on its own line or combined with counts
    std::string remaining = line.substr(3);
    std::istringstream count_stream;
    
    size_t num_vertices = 0;
    size_t num_faces = 0;
    size_t num_edges = 0;
    
    // Try to parse counts from remaining part of first line
    std::istringstream first_line_stream(remaining);
    if (first_line_stream >> num_vertices >> num_faces >> num_edges) {
        // Successfully parsed from first line
    } else {
        // Read counts from next non-empty, non-comment line
        while (std::getline(file, line)) {
            // Trim leading whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            line = line.substr(start);
            
            // Skip comment lines
            if (line[0] == '#') continue;
            
            std::istringstream line_stream(line);
            if (line_stream >> num_vertices >> num_faces >> num_edges) {
                break;
            }
        }
    }
    
    if (num_vertices == 0 || num_faces == 0) {
        LUISA_WARNING_WITH_LOCATION("Invalid OFF file: invalid vertex or face count");
        return false;
    }
    
    MeshBuilder mesh_builder;
    mesh_builder.position.reserve(num_vertices);
    
    // Read vertices
    size_t vertices_read = 0;
    while (vertices_read < num_vertices && std::getline(file, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        // Skip comment lines
        if (line[0] == '#') continue;
        
        std::istringstream vertex_stream(line);
        float x, y, z;
        if (vertex_stream >> x >> y >> z) {
            mesh_builder.position.push_back(make_float3(x, y, z));
            vertices_read++;
        }
    }
    
    if (vertices_read != num_vertices) {
        LUISA_WARNING_WITH_LOCATION(
            "OFF file: expected {} vertices but read {}", 
            num_vertices, 
            vertices_read);
        return false;
    }
    
    // Read faces and convert to triangles
    auto &indices = mesh_builder.triangle_indices.emplace_back();
    size_t faces_read = 0;
    
    while (faces_read < num_faces && std::getline(file, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        // Skip comment lines
        if (line[0] == '#') continue;
        
        std::istringstream face_stream(line);
        size_t n;
        if (!(face_stream >> n)) continue;
        
        if (n < 3) {
            LUISA_WARNING_WITH_LOCATION("OFF file: face with less than 3 vertices");
            continue;
        }
        
        // Read vertex indices for this face
        luisa::vector<size_t> face_indices;
        face_indices.reserve(n);
        size_t index;
        for (size_t i = 0; i < n; ++i) {
            if (!(face_stream >> index)) {
                LUISA_WARNING_WITH_LOCATION("OFF file: incomplete face data");
                break;
            }
            face_indices.push_back(index);
        }
        
        if (face_indices.size() != n) {
            continue;
        }
        
        // Triangulate the face (simple fan triangulation)
        for (size_t i = 1; i + 1 < n; ++i) {
            indices.push_back(static_cast<uint>(face_indices[0]));
            indices.push_back(static_cast<uint>(face_indices[i]));
            indices.push_back(static_cast<uint>(face_indices[i + 1]));
        }
        
        faces_read++;
    }
    
    if (faces_read != num_faces) {
        LUISA_WARNING_WITH_LOCATION(
            "OFF file: expected {} faces but read {}", 
            num_faces, 
            faces_read);
        // Continue anyway if we have some valid data
        if (indices.empty()) {
            return false;
        }
    }
    
    if (mesh_builder.position.empty() || indices.empty()) {
        LUISA_WARNING_WITH_LOCATION("OFF file: no valid mesh data");
        return false;
    }
    
    // Build the mesh resource
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
