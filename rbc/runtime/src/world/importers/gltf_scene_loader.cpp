#include <rbc_world/importers/gltf_scene_loader.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_world/texture_loader.h>
#include <rbc_world/resources/material.h>
#include <tiny_gltf.h>
#include <luisa/core/logging.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool load_gltf_model(tinygltf::Model &model, luisa::filesystem::path const &path, bool is_binary) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    luisa::string luisa_path_str = luisa::to_string(path);
    std::string path_str(luisa_path_str.c_str(), luisa_path_str.size());

    bool ret = false;
    if (is_binary) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path_str);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path_str);
    }

    if (!warn.empty()) {
        LUISA_WARNING_WITH_LOCATION("GLTF warning: {}", warn);
    }

    if (!err.empty()) {
        LUISA_WARNING_WITH_LOCATION("GLTF error: {}", err);
    }

    return ret;
}

GltfSceneData GltfSceneLoader::load_scene(luisa::filesystem::path const &gltf_path) {
    GltfSceneData result;

    // Load GLTF model
    tinygltf::Model model;
    if (!load_gltf_model(model, gltf_path, false)) {
        LUISA_ERROR("Failed to load GLTF file: {}", luisa::to_string(gltf_path));
        return result;
    }

    // Get the directory containing the GLTF file (for resolving relative texture paths)
    auto gltf_dir = gltf_path.parent_path();

    // Load mesh using GltfMeshImporter
    {
        result.mesh = RC<MeshResource>(create_object<MeshResource>());
        GltfMeshImporter importer;
        if (!importer.import(result.mesh.get(), gltf_path)) {
            LUISA_ERROR("Failed to import mesh from GLTF file");
            return result;
        }
    }

    // Load textures and materials
    {
        TextureLoader tex_loader;

        // First pass: load all images (textures reference images)
        // Map from image index to loaded texture resource
        luisa::vector<RC<TextureResource>> image_to_texture;
        image_to_texture.resize(model.images.size());

        for (size_t img_idx = 0; img_idx < model.images.size(); ++img_idx) {
            auto const &img = model.images[img_idx];

            // Resolve texture path
            luisa::filesystem::path tex_path;
            if (!img.uri.empty()) {
                // Check if it's a data URI (base64 encoded)
                if (img.uri.find("data:") == 0) {
                    // Embedded base64 image - skip for now
                    LUISA_WARNING("Base64 embedded images not yet supported, skipping texture");
                    continue;
                }
                // Relative or absolute path
                if (luisa::filesystem::path(img.uri).is_absolute()) {
                    tex_path = img.uri;
                } else {
                    tex_path = gltf_dir / img.uri;
                }
            } else {
                // Embedded image in buffer - skip for now
                LUISA_WARNING("Buffer-embedded images not yet supported, skipping texture");
                continue;
            }

            // Load texture
            auto tex_res = tex_loader.decode_texture(tex_path, 4, false);
            if (tex_res) {
                image_to_texture[img_idx] = std::move(tex_res);
            } else {
                LUISA_WARNING("Failed to load texture: {}", luisa::to_string(tex_path));
            }
        }

        tex_loader.finish_task();

        // Store loaded textures and initialize them
        for (auto &tex : image_to_texture) {
            if (tex) {
                tex->init_device_resource();
                result.textures.push_back(tex);
            }
        }

        // Second pass: create materials
        for (size_t mat_idx = 0; mat_idx < model.materials.size(); ++mat_idx) {
            auto const &gltf_mat = model.materials[mat_idx];
            auto const &pbr = gltf_mat.pbrMetallicRoughness;

            // Create material resource
            auto mat = RC<MaterialResource>(create_object<MaterialResource>());

            // Build material JSON
            luisa::string mat_json = R"({"type": "pbr")";

            // Add base color factor (if present)
            if (!pbr.baseColorFactor.empty() && pbr.baseColorFactor.size() >= 3) {
                mat_json += luisa::format(
                    R"(, "base_albedo": [{}, {}, {}])",
                    pbr.baseColorFactor[0],
                    pbr.baseColorFactor[1],
                    pbr.baseColorFactor[2]);
            }

            // Add base color texture (if present)
            if (pbr.baseColorTexture.index >= 0) {
                auto const &tex_info = pbr.baseColorTexture;
                if (tex_info.index >= 0 && tex_info.index < static_cast<int>(model.textures.size())) {
                    auto const &tex = model.textures[tex_info.index];
                    if (tex.source >= 0 && tex.source < static_cast<int>(model.images.size())) {
                        // Get the texture resource from image index
                        if (tex.source < static_cast<int>(image_to_texture.size()) && image_to_texture[tex.source]) {
                            auto tex_res = image_to_texture[tex.source];
                            // Add texture reference to material
                            auto tex_guid = tex_res->guid();
                            mat_json += luisa::format(
                                R"(, "base_albedo_tex": "{}")",
                                tex_guid.to_base64());
                        }
                    }
                }
            }

            // Add metallic and roughness factors
            if (!TINYGLTF_DOUBLE_EQUAL(pbr.metallicFactor, 1.0)) {
                mat_json += luisa::format(R"(, "weight_metallic": {})", pbr.metallicFactor);
            }
            if (!TINYGLTF_DOUBLE_EQUAL(pbr.roughnessFactor, 1.0)) {
                mat_json += luisa::format(R"(, "specular_roughness": {})", pbr.roughnessFactor);
            }

            mat_json += "}";

            // Load material from JSON
            mat->load_from_json(mat_json);
            result.materials.push_back(std::move(mat));
        }
    }

    // If no materials were loaded, create a default material
    if (result.materials.empty()) {
        auto default_mat = RC<MaterialResource>(create_object<MaterialResource>());
        default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
        result.materials.push_back(std::move(default_mat));
    }

    return result;
}

GltfSceneData GltfSceneLoader::load_scene_binary(luisa::filesystem::path const &glb_path) {
    GltfSceneData result;

    // Load GLB model
    tinygltf::Model model;
    if (!load_gltf_model(model, glb_path, true)) {
        LUISA_ERROR("Failed to load GLB file: {}", luisa::to_string(glb_path));
        return result;
    }

    // Get the directory containing the GLB file (for resolving relative texture paths)
    auto glb_dir = glb_path.parent_path();

    // Load mesh using GlbMeshImporter
    {
        result.mesh = RC<MeshResource>(create_object<MeshResource>());
        GlbMeshImporter importer;
        if (!importer.import(result.mesh.get(), glb_path)) {
            LUISA_ERROR("Failed to import mesh from GLB file");
            return result;
        }
    }

    // Load textures and materials (same logic as load_scene)
    {
        TextureLoader tex_loader;

        // First pass: load all images (textures reference images)
        // Map from image index to loaded texture resource
        luisa::vector<RC<TextureResource>> image_to_texture;
        image_to_texture.resize(model.images.size());

        for (size_t img_idx = 0; img_idx < model.images.size(); ++img_idx) {
            auto const &img = model.images[img_idx];

            // Resolve texture path
            luisa::filesystem::path tex_path;
            if (!img.uri.empty()) {
                // Check if it's a data URI (base64 encoded)
                if (img.uri.find("data:") == 0) {
                    // Embedded base64 image - skip for now
                    LUISA_WARNING("Base64 embedded images not yet supported, skipping texture");
                    continue;
                }
                // Relative or absolute path
                if (luisa::filesystem::path(img.uri).is_absolute()) {
                    tex_path = img.uri;
                } else {
                    tex_path = glb_dir / img.uri;
                }
            } else {
                // Embedded image in buffer - skip for now
                LUISA_WARNING("Buffer-embedded images not yet supported, skipping texture");
                continue;
            }

            // Load texture
            auto tex_res = tex_loader.decode_texture(tex_path, 16, true);
            if (tex_res) {
                image_to_texture[img_idx] = std::move(tex_res);
            } else {
                LUISA_WARNING("Failed to load texture: {}", luisa::to_string(tex_path));
            }
        }

        tex_loader.finish_task();

        // Store loaded textures and initialize them
        for (auto &tex : image_to_texture) {
            if (tex) {
                tex->init_device_resource();
                result.textures.push_back(tex);
            }
        }

        // Second pass: create materials
        for (size_t mat_idx = 0; mat_idx < model.materials.size(); ++mat_idx) {
            auto const &gltf_mat = model.materials[mat_idx];
            auto const &pbr = gltf_mat.pbrMetallicRoughness;

            // Create material resource
            auto mat = RC<MaterialResource>(create_object<MaterialResource>());

            // Build material JSON
            luisa::string mat_json = R"({"type": "pbr")";

            // Add base color factor (if present)
            if (!pbr.baseColorFactor.empty() && pbr.baseColorFactor.size() >= 3) {
                mat_json += luisa::format(
                    R"(, "base_albedo": [{}, {}, {}])",
                    pbr.baseColorFactor[0],
                    pbr.baseColorFactor[1],
                    pbr.baseColorFactor[2]);
            }

            // Add base color texture (if present)
            if (pbr.baseColorTexture.index >= 0) {
                auto const &tex_info = pbr.baseColorTexture;
                if (tex_info.index >= 0 && tex_info.index < static_cast<int>(model.textures.size())) {
                    auto const &tex = model.textures[tex_info.index];
                    if (tex.source >= 0 && tex.source < static_cast<int>(model.images.size())) {
                        // Get the texture resource from image index
                        if (tex.source < static_cast<int>(image_to_texture.size()) && image_to_texture[tex.source]) {
                            auto tex_res = image_to_texture[tex.source];
                            // Add texture reference to material
                            auto tex_guid = tex_res->guid();
                            mat_json += luisa::format(
                                R"(, "base_albedo_tex": "{}")",
                                tex_guid.to_base64());
                        }
                    }
                }
            }

            // Add metallic and roughness factors
            if (!TINYGLTF_DOUBLE_EQUAL(pbr.metallicFactor, 1.0)) {
                mat_json += luisa::format(R"(, "weight_metallic": {})", pbr.metallicFactor);
            }
            if (!TINYGLTF_DOUBLE_EQUAL(pbr.roughnessFactor, 1.0)) {
                mat_json += luisa::format(R"(, "specular_roughness": {})", pbr.roughnessFactor);
            }

            mat_json += "}";

            // Load material from JSON
            mat->load_from_json(mat_json);
            result.materials.push_back(std::move(mat));
        }
    }

    // If no materials were loaded, create a default material
    if (result.materials.empty()) {
        auto default_mat = RC<MaterialResource>(create_object<MaterialResource>());
        default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
        result.materials.push_back(std::move(default_mat));
    }

    return result;
}

}// namespace rbc::world
