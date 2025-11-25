#include "rbc_world/resource_loader.h"
#include "rbc_world/gpu_resource.h"
#include "rbc_world/mesh_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

namespace rbc {

void Mesh::compute_normals() {
    // Simple flat normal computation
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        float v0[3] = {vertices[i0].position[0], vertices[i0].position[1], vertices[i0].position[2]};
        float v1[3] = {vertices[i1].position[0], vertices[i1].position[1], vertices[i1].position[2]};
        float v2[3] = {vertices[i2].position[0], vertices[i2].position[1], vertices[i2].position[2]};

        // Edge vectors
        float e1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
        float e2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};

        // Cross product
        float n[3] = {
            e1[1] * e2[2] - e1[2] * e2[1],
            e1[2] * e2[0] - e1[0] * e2[2],
            e1[0] * e2[1] - e1[1] * e2[0]};

        // Normalize
        float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f) {
            n[0] /= len;
            n[1] /= len;
            n[2] /= len;
        }

        // Assign to all three vertices
        std::memcpy(vertices[i0].normal, n, sizeof(float) * 3);
        std::memcpy(vertices[i1].normal, n, sizeof(float) * 3);
        std::memcpy(vertices[i2].normal, n, sizeof(float) * 3);
    }
}

void Mesh::compute_tangents() {
    // Simple tangent computation (MikkTSpace would be better)
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        // For now, just create an orthogonal tangent to the normal
        for (auto idx : {i0, i1, i2}) {
            float *n = vertices[idx].normal;
            float t[3];

            // Pick arbitrary perpendicular vector
            if (std::abs(n[0]) > 0.9f) {
                t[0] = 0.0f;
                t[1] = 1.0f;
                t[2] = 0.0f;
            } else {
                t[0] = 1.0f;
                t[1] = 0.0f;
                t[2] = 0.0f;
            }

            // Gram-Schmidt orthogonalize
            float dot = t[0] * n[0] + t[1] * n[1] + t[2] * n[2];
            t[0] -= n[0] * dot;
            t[1] -= n[1] * dot;
            t[2] -= n[2] * dot;

            // Normalize
            float len = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
            if (len > 0.0f) {
                t[0] /= len;
                t[1] /= len;
                t[2] /= len;
            }

            vertices[idx].tangent[0] = t[0];
            vertices[idx].tangent[1] = t[1];
            vertices[idx].tangent[2] = t[2];
            vertices[idx].tangent[3] = 1.0f;
        }
    }
}

void Mesh::upload_to_gpu() {
    // TODO: Implement GPU upload
    // This would use the graphics backend (LuisaCompute, Vulkan, DX12, etc.)
    gpu_uploaded = false;
}

bool MeshLoader::can_load(const std::string &path) const {
    return path.ends_with(".rbm") ||
           path.ends_with(".obj");
}

luisa::shared_ptr<void> MeshLoader::load(const std::string &path,
                                         const std::string &options_json) {
    if (path.ends_with(".rbm")) {
        return load_rbm(path);
    } else if (path.ends_with(".obj")) {
        return load_obj(path);
    }
    return nullptr;
}

luisa::shared_ptr<Mesh> MeshLoader::load_rbm(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[MeshLoader] Failed to open: " << path << "\n";
        return nullptr;
    }

    auto mesh = luisa::make_shared<Mesh>();

    // Read header
    struct Header {
        uint32_t magic;// 'RBM\0'
        uint32_t version;
        uint32_t vertex_count;
        uint32_t index_count;
        uint32_t flags;
    } header;

    file.read(reinterpret_cast<char *>(&header), sizeof(header));

    if (header.magic != 0x004D4252) {// 'RBM\0'
        std::cerr << "[MeshLoader] Invalid RBM magic number\n";
        return nullptr;
    }

    // Read vertices
    mesh->vertices.resize(header.vertex_count);
    file.read(reinterpret_cast<char *>(mesh->vertices.data()),
              header.vertex_count * sizeof(Vertex));

    // Read indices
    mesh->indices.resize(header.index_count);
    file.read(reinterpret_cast<char *>(mesh->indices.data()),
              header.index_count * sizeof(uint32_t));

    return mesh;
}

luisa::shared_ptr<Mesh> MeshLoader::load_obj(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[MeshLoader] Failed to open: " << path << "\n";
        return nullptr;
    }

    auto mesh = luisa::make_shared<Mesh>();

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back(x);
            positions.push_back(y);
            positions.push_back(z);
        } else if (prefix == "vn") {
            float x, y, z;
            iss >> x >> y >> z;
            normals.push_back(x);
            normals.push_back(y);
            normals.push_back(z);
        } else if (prefix == "vt") {
            float u, v;
            iss >> u >> v;
            texcoords.push_back(u);
            texcoords.push_back(v);
        } else if (prefix == "f") {
            // Simple face parsing (assumes triangulated)

            for (int i = 0; i < 3; ++i) {
                std::string vert_str;
                iss >> vert_str;

                // Parse v/vt/vn format
                int v_idx = 0, vt_idx = 0, vn_idx = 0;
                sscanf(vert_str.c_str(), "%d/%d/%d", &v_idx, &vt_idx, &vn_idx);

                Vertex vert{};
                if (v_idx > 0 && (size_t)v_idx <= positions.size() / 3) {
                    int idx = (v_idx - 1) * 3;
                    vert.position[0] = positions[idx];
                    vert.position[1] = positions[idx + 1];
                    vert.position[2] = positions[idx + 2];
                }
                if (vn_idx > 0 && (size_t)vn_idx <= normals.size() / 3) {
                    int idx = (vn_idx - 1) * 3;
                    vert.normal[0] = normals[idx];
                    vert.normal[1] = normals[idx + 1];
                    vert.normal[2] = normals[idx + 2];
                }
                if (vt_idx > 0 && (size_t)vt_idx <= texcoords.size() / 2) {
                    int idx = (vt_idx - 1) * 2;
                    vert.texcoord[0] = texcoords[idx];
                    vert.texcoord[1] = texcoords[idx + 1];
                }

                mesh->vertices.push_back(vert);
                mesh->indices.push_back(static_cast<uint32_t>(mesh->vertices.size() - 1));
            }
        }
    }

    if (mesh->vertices.empty()) {
        return nullptr;
    }

    return mesh;
}

luisa::unique_ptr<ResourceLoader> create_mesh_loader() {
    return luisa::make_unique<MeshLoader>();
}

}// namespace rbc
