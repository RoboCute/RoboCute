#include "rbc_world/resource_loader.h"
#include "rbc_world/gpu_resource.h"
#include <fstream>
#include <iostream>
#include <cstring>

// Simple JSON-like parser (placeholder)
// In production, use nlohmann/json or similar

namespace rbc {

class MaterialLoader : public ResourceLoader {
public:
    [[nodiscard]] bool can_load(const std::string &path) const override {
        return path.ends_with(".json") ||
               path.ends_with(".rbm");// RoboCute material format
    }

    [[nodiscard]] luisa::shared_ptr<void> load(const std::string &path,
                                               const std::string &options_json) override {
        if (path.ends_with(".rbm")) {
            return load_rbm(path);
        } else {
            return load_json(path);
        }
    }

    [[nodiscard]] size_t get_resource_size(const luisa::shared_ptr<void> &resource) const override {
        auto material = luisa::static_pointer_cast<Material>(resource);
        return material->get_memory_size();
    }

private:
    [[nodiscard]] luisa::shared_ptr<Material> load_rbm(const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[MaterialLoader] Failed to open: " << path << "\n";
            return nullptr;
        }

        auto material = luisa::make_shared<Material>();

        // Read header
        struct Header {
            uint32_t magic;// 'RBMT'
            uint32_t version;
        } header;

        file.read(reinterpret_cast<char *>(&header), sizeof(header));

        if (header.magic != 0x544D4252) {// 'RBMT' (RoboCute Material)
            std::cerr << "[MaterialLoader] Invalid RBM material magic number\n";
            return nullptr;
        }

        // Read material name length
        uint32_t name_length;
        file.read(reinterpret_cast<char *>(&name_length), sizeof(name_length));

        // Read material name
        material->name.resize(name_length);
        file.read(&material->name[0], name_length);

        // Read PBR parameters
        file.read(reinterpret_cast<char *>(material->base_color), sizeof(float) * 4);
        file.read(reinterpret_cast<char *>(&material->metallic), sizeof(float));
        file.read(reinterpret_cast<char *>(&material->roughness), sizeof(float));
        file.read(reinterpret_cast<char *>(material->emissive), sizeof(float) * 3);

        // Read texture IDs
        file.read(reinterpret_cast<char *>(&material->base_color_texture), sizeof(uint32_t));
        file.read(reinterpret_cast<char *>(&material->metallic_roughness_texture), sizeof(uint32_t));
        file.read(reinterpret_cast<char *>(&material->normal_texture), sizeof(uint32_t));
        file.read(reinterpret_cast<char *>(&material->emissive_texture), sizeof(uint32_t));
        file.read(reinterpret_cast<char *>(&material->occlusion_texture), sizeof(uint32_t));

        return material;
    }

    luisa::shared_ptr<Material> load_json(const std::string &path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[MaterialLoader] Failed to open: " << path << "\n";
            return nullptr;
        }

        auto material = luisa::make_shared<Material>();

        // Simple JSON parsing (very basic, not robust)
        // TODO: Use proper JSON library like nlohmann/json
        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\n\r{},\"");
            if (start == std::string::npos) continue;

            // Look for key-value pairs
            if (line.find("\"name\"") != std::string::npos) {
                size_t colon = line.find(':');
                size_t quote1 = line.find('\"', colon);
                size_t quote2 = line.find('\"', quote1 + 1);
                if (quote1 != std::string::npos && quote2 != std::string::npos) {
                    material->name = line.substr(quote1 + 1, quote2 - quote1 - 1);
                }
            } else if (line.find("\"metallic\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    material->metallic = std::stof(line.substr(colon + 1));
                }
            } else if (line.find("\"roughness\"") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    material->roughness = std::stof(line.substr(colon + 1));
                }
            }
            // Add more parsing as needed...
        }

        if (material->name.empty()) {
            // Extract name from filename
            size_t last_slash = path.find_last_of("/\\");
            size_t last_dot = path.find_last_of('.');
            if (last_slash != std::string::npos && last_dot != std::string::npos) {
                material->name = path.substr(last_slash + 1, last_dot - last_slash - 1);
            } else {
                material->name = "default";
            }
        }

        std::cout << "[MaterialLoader] Loaded material '" << material->name
                  << "' from " << path << "\n";

        return material;
    }
};

// Factory function
luisa::unique_ptr<ResourceLoader> create_material_loader() {
    return luisa::make_unique<MaterialLoader>();
}

}// namespace rbc
