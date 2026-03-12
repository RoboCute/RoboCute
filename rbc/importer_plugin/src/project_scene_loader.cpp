#include <rbc_importer/project_scene_loader.h>
#include <rbc_importer/gltf_scene_loader.h>
#include <rbc_world/entity.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/skelmesh_component.h>
#include <luisa/core/logging.h>
#include <rbc_core/json_serde.h>
#include <fstream>

namespace rbc::world {

ProjectSceneLoader::ProjectSceneLoader(luisa::filesystem::path project_root)
    : _project_root(std::move(project_root)) {
    
    ensure_intermediate_structure();
}

void ProjectSceneLoader::ensure_intermediate_structure() {
    auto intermediate = intermediate_dir();
    if (!luisa::filesystem::exists(intermediate)) {
        luisa::filesystem::create_directories(intermediate);
    }
    
    // Create subdirectories for different resource types
    auto create_subdir = [&](const char *name) {
        auto dir = intermediate / name;
        if (!luisa::filesystem::exists(dir)) {
            luisa::filesystem::create_directories(dir);
        }
    };
    
    create_subdir("meshes");
    create_subdir("materials");
    create_subdir("textures");
    create_subdir("skeletons");
    create_subdir("skins");
    create_subdir("animations");
    create_subdir("scenes");
}

ProjectSceneLoader::LoadResult ProjectSceneLoader::load(
    luisa::filesystem::path const &relative_path,
    LoadConfig const &config) {
    
    LoadResult result;
    
    auto source_path = assets_dir() / relative_path;
    if (!luisa::filesystem::exists(source_path)) {
        result.error = luisa::format("Scene file not found: {}", luisa::to_string(source_path));
        return result;
    }
    
    // Find appropriate importer
    auto *importer = SceneAssetImporterRegistry::instance().find_importer(source_path);
    if (!importer) {
        result.error = luisa::format("No importer found for: {}", luisa::to_string(source_path));
        return result;
    }
    
    LUISA_INFO("Loading scene with {} importer: {}", 
               importer->name(), 
               luisa::to_string(relative_path));
    
    // Check if we need to re-import
    auto import_marker = intermediate_dir() / "scenes" / (relative_path.string() + ".imported");
    bool needs_import = config.force_reimport || 
                        needs_reimport(source_path, import_marker);
    
    if (needs_import) {
        // Perform import
        SceneImportContext ctx;
        ctx.project_root = _project_root;
        ctx.intermediate_dir = intermediate_dir();
        ctx.source_dir = source_path.parent_path();
        ctx.settings = config.import_settings;
        
        auto import_result = importer->import(source_path, ctx);
        
        if (!import_result.success) {
            result.error = import_result.error_message;
            return result;
        }
        
        // Save imported resources
        if (config.auto_save) {
            for (auto &res : import_result.resources) {
                if (res) {
                    res->save_to_path();
                    world::register_resource_meta(res.get());
                }
            }
        }
        
        result.resources = std::move(import_result.resources);
        
        // Write import marker
        write_import_marker(source_path, import_marker);
        
        LUISA_INFO("Scene imported successfully: {} resources", result.resources.size());
    } else {
        // Load from cache
        // TODO: Load previously imported resources from metadata
        LUISA_INFO("Loading cached scene: {}", luisa::to_string(relative_path));
    }
    
    // For GLTF files, use the existing GltfSceneLoader to get the full scene data
    // This ensures compatibility with existing code
    if (importer->name() == "gltf") {
        GltfLoadConfig gltf_config;
        gltf_config.load_skeleton = config.import_settings.import_skeletons;
        gltf_config.load_skin = config.import_settings.import_skins;
        gltf_config.load_anim_seq = config.import_settings.import_animations;
        gltf_config.load_mesh = config.import_settings.import_meshes;
        gltf_config.load_materials = config.import_settings.import_materials;
        
        auto gltf_data = GltfSceneLoader::load_scene(source_path, gltf_config);
        
        // Create a SceneResource and populate it
        auto scene = world::create_object<SceneResource>();
        
        // Create entity from loaded data
        if (gltf_data.skelmesh) {
            auto entity = world::create_object<Entity>();
            
            auto transform = entity->add_component<TransformComponent>();
            transform->set_pos(double3(0, 0, 0), true);
            transform->set_scale(double3(0.2, 0.2, 0.2), true);
            
            auto skelmesh = entity->add_component<SkelMeshComponent>();
            skelmesh->SetRefSkelMesh(gltf_data.skelmesh);
            skelmesh->bind_mats = gltf_data.materials;
            
            // Store entity in scene
            // TODO: Add proper entity management to SceneResource
            
            result.scene = scene;
            result.success = true;
            
            // Collect all resources
            if (gltf_data.mesh) {
                result.resources.push_back(gltf_data.mesh.template cast_static<Resource>());
            }
            if (gltf_data.skel) {
                result.resources.push_back(gltf_data.skel.template cast_static<Resource>());
            }
            if (gltf_data.skin) {
                result.resources.push_back(gltf_data.skin.template cast_static<Resource>());
            }
            if (gltf_data.anim) {
                result.resources.push_back(gltf_data.anim.template cast_static<Resource>());
            }
            if (gltf_data.skelmesh) {
                result.resources.push_back(gltf_data.skelmesh.template cast_static<Resource>());
            }
            if (gltf_data.anim_graph) {
                result.resources.push_back(gltf_data.anim_graph.template cast_static<Resource>());
            }
            for (auto &mat : gltf_data.materials) {
                if (mat) {
                    result.resources.push_back(mat.template cast_static<Resource>());
                }
            }
            for (auto &tex : gltf_data.textures) {
                if (tex) {
                    result.resources.push_back(tex.template cast_static<Resource>());
                }
            }
        } else if (gltf_data.mesh) {
            // Static mesh case
            auto entity = world::create_object<Entity>();
            
            auto transform = entity->add_component<TransformComponent>();
            transform->set_pos(double3(0, 0, 0), true);
            transform->set_scale(double3(0.2, 0.2, 0.2), true);
            
            auto render = entity->add_component<RenderComponent>();
            
            // Ensure materials
            auto materials = gltf_data.materials;
            if (materials.empty()) {
                auto default_mat = RC<MaterialResource>(world::create_object<MaterialResource>());
                default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
                materials.push_back(default_mat);
            }
            
            // Ensure enough materials for submeshes
            while (materials.size() < gltf_data.mesh->submesh_count()) {
                materials.push_back(materials[0]);
            }
            
            render->update_object(materials, gltf_data.mesh.get());
            
            auto scene = world::create_object<SceneResource>();
            result.scene = scene;
            result.success = true;
            
            result.resources.push_back(gltf_data.mesh.template cast_static<Resource>());
            for (auto &mat : materials) {
                if (mat) {
                    result.resources.push_back(mat.template cast_static<Resource>());
                }
            }
            for (auto &tex : gltf_data.textures) {
                if (tex) {
                    result.resources.push_back(tex.template cast_static<Resource>());
                }
            }
        }
    }
    
    return result;
}

bool ProjectSceneLoader::needs_reimport(
    luisa::filesystem::path const &source_path,
    luisa::filesystem::path const &imported_marker) const {
    
    if (!luisa::filesystem::exists(imported_marker)) {
        return true;
    }
    
    // Compare modification times
    auto source_time = luisa::filesystem::last_write_time(source_path);
    auto marker_time = luisa::filesystem::last_write_time(imported_marker);
    
    return source_time > marker_time;
}

void ProjectSceneLoader::write_import_marker(
    luisa::filesystem::path const &source_path,
    luisa::filesystem::path const &marker_path) {
    
    // Ensure parent directory exists
    auto parent = marker_path.parent_path();
    if (!luisa::filesystem::exists(parent)) {
        luisa::filesystem::create_directories(parent);
    }
    
    // Write marker with source file info
    JsonSerializer ser{true};// root_array = true for array at root
    ser._store(luisa::to_string(source_path), "source");
    ser._store(std::chrono::system_clock::now().time_since_epoch().count(), "timestamp");
    
    auto json_blob = ser.write_to();
    
    std::ofstream file(marker_path.string(), std::ios::binary);
    file.write((char const *)json_blob.data(), json_blob.size());
}

// Convenience function
ProjectSceneLoader::LoadResult load_project_scene(
    luisa::filesystem::path const &project_root,
    luisa::filesystem::path const &scene_relative_path,
    ProjectSceneLoader::LoadConfig const &config) {
    
    ProjectSceneLoader loader(project_root);
    return loader.load(scene_relative_path, config);
}

} // namespace rbc::world
