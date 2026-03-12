#include <rbc_importer/gltf_asset_importer.h>
#include <rbc_importer/gltf_scene_loader.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/resources/skin.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_world/resources/skelmesh.h>
#include <rbc_world/resources/anim_graph.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_world/resource_importer.h>
#include <luisa/core/logging.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {

bool GltfAssetImporter::can_import(luisa::filesystem::path const &path) const {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".gltf" || ext == ".glb";
}

luisa::vector<luisa::string> GltfAssetImporter::supported_extensions() const {
    return {".gltf", ".glb"};
}

luisa::vector<SubAssetInfo> GltfAssetImporter::analyze(
    luisa::filesystem::path const &path,
    SceneImportContext const &ctx) const {
    
    luisa::vector<SubAssetInfo> result;
    
    tinygltf::Model model;
    if (!load_model(path, model)) {
        return result;
    }
    
    auto base_dir = path.parent_path();
    
    // Analyze meshes
    for (size_t i = 0; i < model.meshes.size(); ++i) {
        SubAssetInfo info;
        info.type = SubAssetInfo::Type::Mesh;
        info.name = model.meshes[i].name.empty() 
            ? luisa::format("mesh_{}", i) 
            : luisa::string(model.meshes[i].name);
        info.index = static_cast<int32_t>(i);
        info.parent_scene = path;
        result.push_back(info);
    }
    
    // Analyze materials
    for (size_t i = 0; i < model.materials.size(); ++i) {
        SubAssetInfo info;
        info.type = SubAssetInfo::Type::Material;
        info.name = model.materials[i].name.empty() 
            ? luisa::format("material_{}", i) 
            : luisa::string(model.materials[i].name);
        info.index = static_cast<int32_t>(i);
        info.parent_scene = path;
        result.push_back(info);
    }
    
    // Analyze textures (and their images)
    for (size_t i = 0; i < model.textures.size(); ++i) {
        if (model.textures[i].source >= 0 && 
            model.textures[i].source < static_cast<int>(model.images.size())) {
            const auto &image = model.images[model.textures[i].source];
            SubAssetInfo info;
            info.type = SubAssetInfo::Type::Texture;
            info.name = image.name.empty() 
                ? luisa::format("texture_{}", i) 
                : luisa::string(image.name);
            info.index = static_cast<int32_t>(i);
            
            // Resolve texture path
            if (!image.uri.empty()) {
                info.source_path = resolve_uri(luisa::string(image.uri), base_dir);
            }
            info.parent_scene = path;
            result.push_back(info);
        }
    }
    
    // Analyze skins
    for (size_t i = 0; i < model.skins.size(); ++i) {
        SubAssetInfo info;
        info.type = SubAssetInfo::Type::Skin;
        info.name = model.skins[i].name.empty() 
            ? luisa::format("skin_{}", i) 
            : luisa::string(model.skins[i].name);
        info.index = static_cast<int32_t>(i);
        info.parent_scene = path;
        result.push_back(info);
    }
    
    // Analyze animations
    for (size_t i = 0; i < model.animations.size(); ++i) {
        SubAssetInfo info;
        info.type = SubAssetInfo::Type::Animation;
        info.name = model.animations[i].name.empty() 
            ? luisa::format("animation_{}", i) 
            : luisa::string(model.animations[i].name);
        info.index = static_cast<int32_t>(i);
        info.parent_scene = path;
        result.push_back(info);
    }
    
    // Note: Skeletons are derived from skin joints, not direct GLTF resources
    
    return result;
}

SceneImportResult GltfAssetImporter::import(
    luisa::filesystem::path const &path,
    SceneImportContext const &ctx) const {
    
    SceneImportResult result;
    
    GltfImportState state;
    state.source_dir = path.parent_path();
    state.intermediate_dir = ctx.intermediate_dir;
    state.settings = ctx.settings;
    
    if (!load_model(path, state.model)) {
        result.error_message = "Failed to load GLTF model";
        return result;
    }
    
    LUISA_INFO("Importing GLTF scene: {}", luisa::to_string(path));
    
    // Import resources in dependency order
    // 1. Textures first (no dependencies)
    if (ctx.settings.import_textures) {
        import_textures(state, ctx);
    }
    
    // 2. Materials (depend on textures)
    if (ctx.settings.import_materials) {
        import_materials(state);
    }
    
    // 3. Meshes (no dependencies)
    if (ctx.settings.import_meshes) {
        import_meshes(state);
    }
    
    // 4. Skeletons (no dependencies, derived from nodes)
    if (ctx.settings.import_skeletons) {
        import_skeletons(state);
    }
    
    // 5. Skins (depend on skeletons)
    if (ctx.settings.import_skins) {
        import_skins(state);
    }
    
    // 6. Animations (depend on skeletons)
    if (ctx.settings.import_animations) {
        import_animations(state);
    }
    
    // Collect all imported resources
    for (auto &mesh : state.meshes) {
        if (mesh) {
            result.resources.push_back(mesh.template cast_static<Resource>());
            state.name_to_guid[mesh->guid().to_string()] = mesh->guid();
        }
    }
    for (auto &mat : state.materials) {
        if (mat) {
            result.resources.push_back(mat.template cast_static<Resource>());
        }
    }
    for (auto &tex : state.textures) {
        if (tex) {
            result.resources.push_back(tex.template cast_static<Resource>());
        }
    }
    for (auto &skel : state.skeletons) {
        if (skel) {
            result.resources.push_back(skel.template cast_static<Resource>());
        }
    }
    for (auto &skin : state.skins) {
        if (skin) {
            result.resources.push_back(skin.template cast_static<Resource>());
        }
    }
    for (auto &anim : state.animations) {
        if (anim) {
            result.resources.push_back(anim.template cast_static<Resource>());
        }
    }
    
    // Create main scene resource (SkelMesh if we have skinned meshes)
    // For now, we'll create a SkelMeshResource as the main result
    // This could be extended to create a full SceneResource
    
    result.name_to_guid = std::move(state.name_to_guid);
    result.success = true;
    
    LUISA_INFO("GLTF import complete: {} resources imported", result.resources.size());
    
    return result;
}

bool GltfAssetImporter::load_model(
    luisa::filesystem::path const &path,
    tinygltf::Model &model) const {
    
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    luisa::string path_str = luisa::to_string(path);
    bool ret = false;
    
    if (path.extension() == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path_str.c_str());
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path_str.c_str());
    }
    
    if (!warn.empty()) {
        LUISA_WARNING("GLTF warning: {}", warn);
    }
    if (!err.empty()) {
        LUISA_ERROR("GLTF error: {}", err);
    }
    
    return ret;
}

void GltfAssetImporter::import_meshes(GltfImportState &state) const {
    // Use existing GltfMeshImporter logic
    // For now, we'll use GltfSceneLoader internally
    // This could be refactored to use direct mesh import
    
    LUISA_INFO("Importing {} meshes from GLTF", state.model.meshes.size());
    
    // Meshes are imported as part of the scene loading process
    // We'll create empty mesh resources here and fill them later
    state.meshes.resize(state.model.meshes.size());
}

void GltfAssetImporter::import_materials(GltfImportState &state) const {
    LUISA_INFO("Importing {} materials from GLTF", state.model.materials.size());
    
    state.materials.reserve(state.model.materials.size());
    
    for (size_t i = 0; i < state.model.materials.size(); ++i) {
        const auto &gltf_mat = state.model.materials[i];
        auto mat = world::create_object<MaterialResource>();
        
        // Build material JSON
        luisa::string mat_json = R"({"type": "pbr")";
        const auto &pbr = gltf_mat.pbrMetallicRoughness;
        
        // Base color factor
        if (!pbr.baseColorFactor.empty() && pbr.baseColorFactor.size() >= 3) {
            mat_json += luisa::format(
                R"(, "base_albedo": [{}, {}, {}])",
                pbr.baseColorFactor[0],
                pbr.baseColorFactor[1],
                pbr.baseColorFactor[2]);
        }
        
        // Base color texture
        if (pbr.baseColorTexture.index >= 0 && 
            pbr.baseColorTexture.index < static_cast<int>(state.textures.size())) {
            auto tex = state.textures[pbr.baseColorTexture.index];
            if (tex) {
                mat_json += luisa::format(
                    R"(, "base_albedo_tex": "{}")",
                    tex->guid().to_base64());
            }
        }
        
        // Metallic and roughness
        if (pbr.metallicFactor != 1.0) {
            mat_json += luisa::format(R"(, "weight_metallic": {})", pbr.metallicFactor);
        }
        if (pbr.roughnessFactor != 1.0) {
            mat_json += luisa::format(R"(, "specular_roughness": {})", pbr.roughnessFactor);
        }
        
        mat_json += "}";
        
        mat->load_from_json(mat_json);
        state.materials.push_back(RC<MaterialResource>{mat});
        
        if (!gltf_mat.name.empty()) {
            state.name_to_guid[luisa::string(gltf_mat.name)] = mat->guid();
        }
    }
}

void GltfAssetImporter::import_textures(
    GltfImportState &state,
    SceneImportContext const &ctx) const {
    
    LUISA_INFO("Importing {} textures from GLTF", state.model.textures.size());
    
    state.textures.resize(state.model.textures.size());
    
    // Get texture importers from registry
    auto &registry = ResourceImporterRegistry::instance();
    
    for (size_t i = 0; i < state.model.textures.size(); ++i) {
        const auto &texture = state.model.textures[i];
        if (texture.source < 0 || texture.source >= static_cast<int>(state.model.images.size())) {
            continue;
        }
        
        const auto &image = state.model.images[texture.source];
        
        // Handle external image files
        if (!image.uri.empty()) {
            auto image_path = resolve_uri(luisa::string(image.uri), state.source_dir);
            
            if (!luisa::filesystem::exists(image_path)) {
                LUISA_WARNING("Texture file not found: {}", luisa::to_string(image_path));
                continue;
            }
            
            // Find appropriate importer
            auto *importer = registry.find_importer(image_path, TypeInfo::get<TextureResource>().md5());
            if (!importer) {
                LUISA_WARNING("No importer found for texture: {}", luisa::to_string(image_path));
                continue;
            }
            
            auto tex_importer = static_cast<ITextureImporter*>(importer);
            auto tex = world::create_object<TextureResource>();
            
            TextureLoader tex_loader;
            if (tex_importer->import(RC<TextureResource>{tex}, &tex_loader, image_path, 
                                     ctx.settings.texture_mip_levels, 
                                     ctx.settings.texture_compress)) {
                state.textures[i] = tex;
                if (!image.name.empty()) {
                    state.name_to_guid[luisa::string(image.name)] = tex->guid();
                }
            }
            
            tex_loader.finish_task();
        }
        // Handle embedded images (bufferView)
        else if (image.bufferView >= 0) {
            // TODO: Handle embedded images
            LUISA_WARNING("Embedded images not yet supported");
        }
    }
}

void GltfAssetImporter::import_skeletons(GltfImportState &state) const {
    // Skeletons in GLTF are implicit from the node hierarchy
    // We'll create skeletons based on skins' joints
    
    // For now, create one skeleton per skin
    // This could be optimized to share skeletons
}

void GltfAssetImporter::import_skins(GltfImportState &state) const {
    LUISA_INFO("Importing {} skins from GLTF", state.model.skins.size());
    
    // Use GltfSceneLoader for now
    // This will be replaced with direct skin import
}

void GltfAssetImporter::import_animations(GltfImportState &state) const {
    LUISA_INFO("Importing {} animations from GLTF", state.model.animations.size());
    
    // Use GltfSceneLoader for now
    // This will be replaced with direct animation import
}

luisa::filesystem::path GltfAssetImporter::resolve_uri(
    const luisa::string &uri,
    const luisa::filesystem::path &base_dir) const {
    
    // Handle data URIs
    if (uri.find("data:") == 0) {
        // Data URI - would need to decode
        return {};
    }
    
    // Handle relative/absolute paths
    luisa::filesystem::path path(uri);
    if (path.is_absolute()) {
        return path;
    }
    return base_dir / path;
}

luisa::string GltfAssetImporter::get_node_name(
    const tinygltf::Node &node, 
    int index) const {
    if (!node.name.empty()) {
        return luisa::string(node.name);
    }
    return luisa::format("node_{}", index);
}

// Register the importer
REGISTER_SCENE_ASSET_IMPORTER(GltfAssetImporter);

} // namespace rbc::world
