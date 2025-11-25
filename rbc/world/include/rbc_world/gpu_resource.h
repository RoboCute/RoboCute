#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace rbc {

struct Vertex {
    float position[3];
    float normal[3];
    float texcoord[2];
    float tangent[4];// xyz = tangent, w = bitangent sign
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // GPU buffers (opaque handles, can be device pointers or buffer IDs)
    void *vertex_buffer = nullptr;
    void *index_buffer = nullptr;

    // Metadata
    std::string name;
    bool gpu_uploaded = false;

    [[nodiscard]] size_t get_memory_size() const {
        return vertices.size() * sizeof(Vertex) +
               indices.size() * sizeof(uint32_t);
    }

    void compute_normals();
    void compute_tangents();
    void upload_to_gpu();
};

// === Texture Data ===

enum class TextureFormat : uint8_t {
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    BC1,// DXT1
    BC3,// DXT5
    BC5,// Normal maps
    BC7,// High quality
};

struct Texture {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;// For 3D textures
    uint32_t mip_levels = 1;
    TextureFormat format = TextureFormat::RGBA8;

    std::vector<uint8_t> data;

    // GPU handle
    void *gpu_texture = nullptr;
    bool gpu_uploaded = false;

    std::string name;

    size_t get_memory_size() const {
        return data.size();
    }

    void upload_to_gpu();
    void generate_mipmaps();
};

// === Material Data ===

struct Material {
    std::string name;

    // PBR parameters
    float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};

    // Texture IDs (ResourceIDs)
    uint32_t base_color_texture = 0;
    uint32_t metallic_roughness_texture = 0;
    uint32_t normal_texture = 0;
    uint32_t emissive_texture = 0;
    uint32_t occlusion_texture = 0;

    [[nodiscard]] static size_t get_memory_size() { return sizeof(Material); }
};

}// namespace rbc
