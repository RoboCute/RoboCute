#include <rbc_world/util/gltf_scene_loader.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_graphics/texture/texture_loader.h>
#include <rbc_world/resources/material.h>

#include <rbc_world/importers/skel_importer_gltf.h>
#include <rbc_world/importers/anim_sequence_importer_gltf.h>
#include <rbc_world/importers/skin_importer_gltf.h>

#include <rbc_anim/graph/AnimNode_Root.h>
#include <rbc_anim/graph/AnimNode_SequencePlayer.h>

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

GltfSceneData GltfSceneLoader::load_from_model(
    tinygltf::Model &model,
    GltfLoadConfig &config,
    luisa::filesystem::path const &path) {
    GltfSceneData result;
    // Get the directory containing the GLTF file (for resolving relative texture paths)
    auto gltf_dir = path.parent_path();
    result.config = config;

    // Load mesh using GltfMeshImporter
    {
        result.mesh = RC<MeshResource>(create_object<MeshResource>());
        GltfMeshImporter importer;
        if (!importer.import(result.mesh.get(), path)) {
            LUISA_ERROR("Failed to import mesh from GLTF file");
            return result;
        }
    }

    // Load skeleton
    if (config.load_skeleton) {
        result.skel = RC<SkeletonResource>(create_object<SkeletonResource>());
        GltfSkeletonImporter importer;
        if (!importer.import(result.skel.get(), path)) {
            LUISA_ERROR("Failed to import skeleton from GLTF file");
            return result;
        }
        result.skel->unsafe_set_loaded();

        // Load skin (depends on skeleton and mesh)
        if (config.load_skin) {
            result.skin = RC<SkinResource>(create_object<SkinResource>());
            GltfSkinImporter importer;
            if (!importer.import(result.skin.get(), path)) {
                LUISA_ERROR("Failed to import skin from GLTF file");
                return result;
            }
            result.skin->ref_skel = result.skel;
            result.skin->ref_mesh = result.mesh;
            result.skin->generate_LUT();
            result.skin->unsafe_set_loaded();
        }

        // Load animation (depends on skeleton)
        if (config.load_anim_seq) {
            result.anim = create_object<AnimSequenceResource>();
            GltfAnimSequenceImporter importer;
            importer.ref_skel = result.skel;
            if (!importer.import(result.anim.get(), path)) {
                LUISA_ERROR("Failed to import animation sequence from GLTF file");
                return result;
            }
        }
    }

    // Load Texture and Materials
    {
        TextureLoader tex_loader;
        auto &registry = ResourceImporterRegistry::instance();

        for (auto i = 0; i < model.images.size(); i++) {
            result.textures.emplace_back(create_object<TextureResource>());
            auto const &img = model.images[i];

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

            // import with config
            if (!luisa::filesystem::exists(tex_path)) {
                LUISA_WARNING("{} is not valid, check gltf resource {}",
                              tex_path.string(), path.string());
                continue;
            }
            auto *importer = registry.find_importer(tex_path, TypeInfo::get<TextureResource>().md5());

            if (!importer) {
                LUISA_WARNING("No importer found for texture file: {}", tex_path.string());
                continue;
            }
            if (importer->resource_type() != TypeInfo::get<TextureResource>().md5()) {
                LUISA_WARNING("Invalid importer type for texture file: {}", tex_path.string());
                continue;
            }
            auto *texture_importer = static_cast<ITextureImporter *>(importer);

            if (!texture_importer->import(result.textures[i], &tex_loader, tex_path, 4, false)) {
                LUISA_ERROR("{} import failed!", tex_path.string());
                continue;
            }
        }
        tex_loader.finish_task();
        // Store loaded textures and initialize them
        for (auto &tex : result.textures) {
            if (tex) {
                tex->install();
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
                        if (tex.source < static_cast<int>(result.textures.size()) && result.textures[tex.source]) {
                            auto tex_res = result.textures[tex.source];
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

    {
        // Create a Simple Animation Graph
        result.anim_graph = create_object<AnimGraphResource>();
        // nodes[0] is the root node of this AnimGraph
        auto root = RC<rbc::AnimNode_Root>::New();
        result.anim_graph->graph.nodes.emplace_back(root);
        auto seq_player_node = RC<rbc::AnimNode_SequencePlayer>::New();
        seq_player_node->anim_seq_resource = result.anim;
        result.anim_graph->graph.nodes.emplace_back(seq_player_node);
        root->result.LinkedNodeID = 1;// Linked Node Index
        result.anim_graph->unsafe_set_loaded();
    }

    {
        result.skelmesh = create_object<SkelMeshResource>();
        result.skelmesh->ref_skin = result.skin;
        result.skelmesh->ref_skeleton = result.skel;
        result.skelmesh->ref_anim_graph = result.anim_graph;
        result.skelmesh->unsafe_set_loaded();
    }

    return result;
}

GltfSceneData GltfSceneLoader::load_scene(luisa::filesystem::path const &gltf_path, GltfLoadConfig config) {
    // Load GLTF model
    tinygltf::Model model;
    if (!load_gltf_model(model, gltf_path, false)) {
        LUISA_ERROR("Failed to load GLTF file: {}", luisa::to_string(gltf_path));
        return {};
    }
    return load_from_model(model, config, gltf_path);
}

GltfSceneData GltfSceneLoader::load_scene_binary(luisa::filesystem::path const &glb_path, GltfLoadConfig config) {
    tinygltf::Model model;
    if (!load_gltf_model(model, glb_path, true)) {
        LUISA_ERROR("Failed to load GLB file: {}", luisa::to_string(glb_path));
        return {};
    }

    return load_from_model(model, config, glb_path);
}

}// namespace rbc::world
