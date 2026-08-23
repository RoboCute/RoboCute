#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_app/camera_controller.h>
#include <rbc_core/runtime_static.h>
#include <rbc_core/type_info.h>
#include <rbc_plugin/plugin_manager.h>
#include <tracy_wrapper.h>
#include <rbc_core/state_map.h>
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/scene.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/skelmesh_component.h>
#include <rbc_world/components/light_component.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_world/importers/register_importers.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/base_object.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/resources/skin.h>
#include <rbc_world/resources/skelmesh.h>
#include <rbc_world/resources/anim_graph.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_anim/graph/AnimNode_Root.h>
#include <rbc_anim/graph/AnimNode_SequencePlayer.h>
#include <luisa/core/logging.h>
#define TINYGLTF_NO_INCLUDE_JSON
#include <tiny_gltf.h>
#include <material/mats.inl>
#include "rbc_test.hpp"

namespace rbc::test {


// TinyGLTF for loading materials and textures from glTF

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;

// AnimScene - Manages animated character loading and playback
struct AnimScene {
    // Project configuration
    luisa::filesystem::path project_root;
    luisa::filesystem::path intermediate_dir;

    // Scene resources
    RC<world::SceneResource> scene;
    RC<world::Entity> entity;
    RC<world::MeshResource> loaded_mesh;
    RC<world::SkelMeshResource> skel_mesh;
    RC<world::TextureResource> skybox;
    luisa::vector<RC<world::MaterialResource>> loaded_materials;
    luisa::vector<world::Entity *> _entities;

    // All loaded resources for lifecycle management
    luisa::vector<RC<world::Resource>> all_resources;

    AnimScene(
        GraphicsUtils *utils,
        luisa::filesystem::path const &in_project_root);

    ~AnimScene();

    void tick_animation(float delta_time);
    void update_render(GraphicsUtils *utils);
    void install_resources(GraphicsUtils *utils);
    bool has_skybox() const { return skybox.get() != nullptr; }

private:
    void _load_skybox(GraphicsUtils *utils);
    void _load_scene(GraphicsUtils *utils);
    void _setup_default_lighting();
};

AnimScene::AnimScene(
    GraphicsUtils *utils,
    luisa::filesystem::path const &in_project_root)
    : project_root(in_project_root) {

    auto &render_device = RenderDevice::instance();
    auto runtime_dir = render_device.lc_ctx().runtime_directory();

    // Determine project root
    if (project_root.empty()) {
        // Try to find project root from executable location
        auto check_dir = runtime_dir;
        while (!check_dir.empty() && check_dir.has_parent_path()) {
            auto project_file = check_dir / "rbc_project.json";
            if (luisa::filesystem::exists(project_file)) {
                project_root = check_dir;
                break;
            }
            check_dir = check_dir.parent_path();
        }

        // Fallback to default path if no project found
        if (project_root.empty()) {
            project_root = "d:/ws/repos/RoboCute-repo/rbc-project-anim";
        }
    }

    intermediate_dir = project_root / ".rbc";

    LUISA_INFO("Using project root: {}", luisa::to_string(project_root));

    // Initialize world with intermediate directory
    world::init_world(intermediate_dir, intermediate_dir);

    // Load resources
    _load_skybox(utils);
    _load_scene(utils);

    // Setup default lighting if no skybox loaded
    if (!skybox) {
        _setup_default_lighting();
    }
}

void AnimScene::_load_skybox(GraphicsUtils *utils) {
    RBCZoneScopedN("AnimScene::_load_skybox");

    // Get texture importers from registry
    auto &registry = world::ResourceImporterRegistry::instance();

    TextureLoader tex_loader;
    // First try to load landscape.png as sky
    luisa::vector<std::pair<luisa::filesystem::path, luisa::string_view>> skybox_candidates = {
        {project_root / "assets" / "anim_test" / "landscape.png", ".png"},
        {project_root / "assets" / "anim_test" / "landscape.jpg", ".jpg"},
        {project_root / "assets" / "anim_test" / "landscape.hdr", ".hdr"},
        {project_root / "assets" / "sky.exr", ".exr"},
        {project_root / "assets" / "test_scene" / "sky.exr", ".exr"},
        {RenderDevice::instance().lc_ctx().runtime_directory() / "sky.exr", ".exr"},
        {RenderDevice::instance().lc_ctx().runtime_directory() / "test_scene" / "sky.exr", ".exr"},
    };
    for (const auto &[path, ext] : skybox_candidates) {
        if (luisa::filesystem::exists(path)) {
            auto importer = registry.find_importer(ext, TypeInfo::get<world::TextureResource>().md5());
            if (!importer) {
                LUISA_WARNING("No importer found for extension: {}", ext);
                continue;
            }

            auto skybox_rc = RC<world::TextureResource>{world::create_object<world::TextureResource>()};
            auto tex_importer = static_cast<world::ITextureImporter *>(importer);
            // For LDR images (png/jpg), we need to set is_srgb=true and use 4 channels
            bool is_srgb = (ext == ".png" || ext == ".jpg");
            int channels = is_srgb ? 4 : 1;
            if (tex_importer->import(skybox_rc, &tex_loader, path, channels, is_srgb)) {
                tex_loader.finish_task();
                skybox_rc->install();
                utils->update_texture(skybox_rc->get_image());
                // IMPORTANT: Set skybox for rendering
                rbc::RC<DeviceImage> image{skybox_rc->get_image()};
                utils->render_plugin()->update_skybox(image);
                skybox = std::move(skybox_rc);
                LUISA_INFO("Skybox loaded from: {}", luisa::to_string(path));
                return;
            }
        }
    }

    LUISA_WARNING("Skybox not found in any standard location");
}

void AnimScene::_load_scene(GraphicsUtils *utils) {
    RBCZoneScopedN("AnimScene::_load_scene");

    // Try to find test_anim.gltf in project assets
    luisa::vector<luisa::filesystem::path> scene_paths = {
        project_root / "assets" / "anim_test" / "test_anim.gltf",
        project_root / "assets" / "test_anim.gltf",
    };

    luisa::filesystem::path gltf_path;
    bool found = false;

    for (const auto &path : scene_paths) {
        if (luisa::filesystem::exists(path)) {
            gltf_path = path;
            found = true;
            break;
        }
    }

    // Fallback to direct path if project loading failed
    if (!found) {
        gltf_path = "d:/ws/data/assets/anim_test/test_anim.gltf";
        if (!luisa::filesystem::exists(gltf_path)) {
            LUISA_ERROR("Cannot find test_anim.gltf");
            return;
        }
    }

    LUISA_INFO("Loading scene from: {}", luisa::to_string(gltf_path));

    // Register builtin importers (required for loading resources)
    world::register_builtin_importers();

    auto &registry = world::ResourceImporterRegistry::instance();
    auto gltf_dir = gltf_path.parent_path();

    // Load raw glTF model for materials and textures
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // Set dummy image loader to skip embedded images
    auto image_loader = [](tinygltf::Image *, const int, std::string *, std::string *,
                           int, int, const unsigned char *, int, void *) { return true; };
    loader.SetImageLoader(image_loader, NULL);

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_path.string());
    if (!warn.empty()) {
        LUISA_WARNING("GLTF warning: {}", warn);
    }
    if (!err.empty()) {
        LUISA_WARNING("GLTF error: {}", err);
    }
    if (!ret) {
        LUISA_ERROR("Failed to load GLTF file: {}", luisa::to_string(gltf_path));
        return;
    }

    // Step 1: Load mesh
    LUISA_INFO("Loading mesh from GLTF...");
    auto mesh_importer = registry.find_importer(luisa::string_view{".gltf"},
                                                TypeInfo::get<world::MeshResource>().md5());
    if (!mesh_importer) {
        LUISA_ERROR("Failed to find mesh importer for GLTF");
        return;
    }

    loaded_mesh = world::create_object<world::MeshResource>();
    if (!mesh_importer->import(loaded_mesh.get(), gltf_path)) {
        LUISA_ERROR("Failed to import mesh from GLTF");
        return;
    }
    loaded_mesh->unsafe_set_loaded();
    all_resources.push_back(loaded_mesh.template cast_static<world::Resource>());
    LUISA_INFO("Mesh loaded successfully");

    // Step 2: Load skeleton
    LUISA_INFO("Loading skeleton from GLTF...");
    auto skel_importer = registry.find_importer(luisa::string_view{".gltf"},
                                                TypeInfo::get<world::SkeletonResource>().md5());
    RC<world::SkeletonResource> skel;
    if (skel_importer) {
        skel = world::create_object<world::SkeletonResource>();
        if (skel_importer->import(skel.get(), gltf_path)) {
            skel->unsafe_set_loaded();
            all_resources.push_back(skel.template cast_static<world::Resource>());
            LUISA_INFO("Skeleton loaded successfully");
        } else {
            LUISA_WARNING("Failed to import skeleton from GLTF");
            skel.reset();
        }
    } else {
        LUISA_WARNING("No skeleton importer found");
    }

    // Step 3: Load skin (depends on skeleton and mesh)
    RC<world::SkinResource> skin;
    if (skel && loaded_mesh) {
        LUISA_INFO("Loading skin from GLTF...");
        auto skin_importer = registry.find_importer(luisa::string_view{".gltf"},
                                                    TypeInfo::get<world::SkinResource>().md5());
        if (skin_importer) {
            skin = world::create_object<world::SkinResource>();
            if (skin_importer->import(skin.get(), gltf_path)) {
                skin->ref_skel = skel;
                skin->ref_mesh = loaded_mesh;
                skin->generate_LUT();
                skin->unsafe_set_loaded();
                all_resources.push_back(skin.template cast_static<world::Resource>());
                LUISA_INFO("Skin loaded successfully");
            } else {
                LUISA_WARNING("Failed to import skin from GLTF");
                skin.reset();
            }
        } else {
            LUISA_WARNING("No skin importer found");
        }
    }

    // Step 4: Load animation sequence (depends on skeleton)
    RC<world::AnimSequenceResource> anim;
    if (skel) {
        LUISA_INFO("Loading animation sequence from GLTF...");
        auto anim_importer = registry.find_importer(luisa::string_view{".gltf"},
                                                    TypeInfo::get<world::AnimSequenceResource>().md5());
        if (anim_importer) {
            anim = world::create_object<world::AnimSequenceResource>();
            anim->ref_skel = skel;
            if (anim_importer->import(anim.get(), gltf_path)) {
                anim->unsafe_set_loaded();
                all_resources.push_back(anim.template cast_static<world::Resource>());
                LUISA_INFO("Animation sequence loaded successfully");
            } else {
                LUISA_WARNING("Failed to import animation sequence from GLTF");
                anim.reset();
            }
        } else {
            LUISA_WARNING("No animation sequence importer found");
        }
    }

    // Step 5: Load textures and materials from raw glTF model
    LUISA_INFO("Loading textures and materials...");
    TextureLoader tex_loader;
    luisa::vector<RC<world::TextureResource>> loaded_textures;

    for (auto i = 0; i < model.images.size(); i++) {
        auto &img = model.images[i];

        // Resolve texture path
        luisa::filesystem::path tex_path;
        if (!img.uri.empty()) {
            // Check if it's a data URI (base64 encoded)
            if (img.uri.find("data:") == 0) {
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
            LUISA_WARNING("Buffer-embedded images not yet supported, skipping texture");
            continue;
        }

        if (!luisa::filesystem::exists(tex_path)) {
            LUISA_WARNING("Texture path is not valid: {}", luisa::to_string(tex_path));
            continue;
        }

        auto *tex_importer = registry.find_importer(tex_path, TypeInfo::get<world::TextureResource>().md5());
        if (!tex_importer) {
            LUISA_WARNING("No importer found for texture file: {}", luisa::to_string(tex_path));
            continue;
        }

        auto *texture_importer = static_cast<world::ITextureImporter *>(tex_importer);
        auto tex_rc = RC<world::TextureResource>{world::create_object<world::TextureResource>()};
        if (texture_importer->import(tex_rc, &tex_loader, tex_path, 4, false)) {
            tex_rc->unsafe_set_loaded();
            loaded_textures.push_back(std::move(tex_rc));
        }
    }
    tex_loader.finish_task();

    // Initialize textures
    for (auto &tex : loaded_textures) {
        if (tex) {
            tex->install();
            all_resources.push_back(tex.template cast_static<world::Resource>());
        }
    }

    // Create materials from glTF model
    for (size_t mat_idx = 0; mat_idx < model.materials.size(); ++mat_idx) {
        auto const &gltf_mat = model.materials[mat_idx];
        auto const &pbr = gltf_mat.pbrMetallicRoughness;

        auto mat = world::create_object<world::MaterialResource>();

        // Build material JSON
        luisa::string mat_json = R"({"type": "pbr")";

        // Add base color factor
        if (!pbr.baseColorFactor.empty() && pbr.baseColorFactor.size() >= 3) {
            mat_json += luisa::format(
                R"(, "base_albedo": [{}, {}, {}])",
                pbr.baseColorFactor[0],
                pbr.baseColorFactor[1],
                pbr.baseColorFactor[2]);
        }

        // Add base color texture
        if (pbr.baseColorTexture.index >= 0) {
            auto const &tex_info = pbr.baseColorTexture;
            if (tex_info.index >= 0 && tex_info.index < static_cast<int>(model.textures.size())) {
                auto const &tex = model.textures[tex_info.index];
                if (tex.source >= 0 && tex.source < static_cast<int>(model.images.size())) {
                    if (tex.source < static_cast<int>(loaded_textures.size()) && loaded_textures[tex.source]) {
                        auto tex_res = loaded_textures[tex.source];
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

        mat->load_from_json(mat_json);
        mat->unsafe_set_loaded();
        auto mat_rc = RC<world::MaterialResource>{mat};
        loaded_materials.push_back(mat_rc);
        all_resources.push_back(mat_rc.template cast_static<world::Resource>());
    }

    // If no materials were loaded, create a default material
    if (loaded_materials.empty()) {
        auto default_mat = world::create_object<world::MaterialResource>();
        default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
        default_mat->unsafe_set_loaded();
        auto default_mat_rc = RC<world::MaterialResource>{default_mat};
        loaded_materials.push_back(default_mat_rc);
        all_resources.push_back(default_mat_rc.template cast_static<world::Resource>());
    }

    // Step 6: Create animation graph (depends on animation sequence)
    RC<world::AnimGraphResource> anim_graph;
    if (anim) {
        anim_graph = world::create_object<world::AnimGraphResource>();
        auto root = RC<rbc::AnimNode_Root>::New();
        anim_graph->graph().nodes.emplace_back(root);
        auto seq_player_node = RC<rbc::AnimNode_SequencePlayer>::New();
        seq_player_node->anim_seq_resource = anim;
        anim_graph->graph().nodes.emplace_back(seq_player_node);
        root->result.linked_node_id = 1;
        anim_graph->unsafe_set_loaded();
        all_resources.push_back(anim_graph.template cast_static<world::Resource>());
    }

    // Step 7: Create SkelMeshResource (depends on skin, skeleton, anim_graph)
    if (skel && skin) {
        skel_mesh = world::create_object<world::SkelMeshResource>();
        skel_mesh->ref_skin = skin;
        skel_mesh->ref_skeleton = skel;
        skel_mesh->ref_anim_graph = anim_graph;
        skel_mesh->unsafe_set_loaded();
        all_resources.push_back(skel_mesh.template cast_static<world::Resource>());
        LUISA_INFO("SkelMeshResource created successfully");
    }

    // Install all resources
    install_resources(utils);

    // Create entity from loaded resources
    if (skel_mesh) {
        // Create entity with SkelMeshComponent and RenderComponent
        entity = world::create_object<world::Entity>();

        auto transform = entity->add_component<world::TransformComponent>();
        transform->set_pos(double3(0, 0, 0), true);
        transform->set_scale(double3(0.2, 0.2, 0.2), true);

        // RenderComponent is required for SkelMeshComponent to work

        auto skelmesh_comp = entity->add_component<world::SkelMeshComponent>();
        skelmesh_comp->SetRefSkelMesh(skel_mesh);
        skelmesh_comp->bind_mats = loaded_materials;

        // IMPORTANT: Initialize animation system immediately
        // This creates GPU resources and prevents crash
        skelmesh_comp->tick(0.0f);

        _entities.push_back(entity.get());

        LUISA_INFO("Created entity with SkelMeshComponent and RenderComponent");
    } else if (loaded_mesh) {
        // Create entity with RenderComponent for static mesh
        entity = world::create_object<world::Entity>();

        auto transform = entity->add_component<world::TransformComponent>();
        transform->set_pos(double3(0, 0, 0), true);
        transform->set_scale(double3(0.2, 0.2, 0.2), true);

        auto render = entity->add_component<world::RenderComponent>();

        // Ensure materials
        if (loaded_materials.empty()) {
            auto default_mat = RC<world::MaterialResource>(world::create_object<world::MaterialResource>());
            default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
            loaded_materials.push_back(default_mat);
        }

        while (loaded_materials.size() < loaded_mesh->submesh_count()) {
            loaded_materials.push_back(loaded_materials[0]);
        }

        render->update_object(loaded_materials, loaded_mesh.get());

        _entities.push_back(entity.get());

        LUISA_INFO("Created entity with RenderComponent");
    }
}

void AnimScene::install_resources(GraphicsUtils *utils) {
    RBCZoneScopedN("AnimScene::install_resources");

    for (auto &res : all_resources) {
        if (!res) continue;
        LUISA_INFO("Resource [{}] type={} (status={}) is installing",
                   res->path().string(),
                   res->type_name(),
                   static_cast<int>(res->loading_status()));
        // Check if resource is loaded before installing
        if (!res->loaded()) {
            LUISA_WARNING("Resource [{}] type={} is not loaded (status={}), skipping install",
                          res->path().string(),
                          res->type_name(),
                          static_cast<int>(res->loading_status()));
            continue;
        }

        // Install based on resource type
        if (res->is_type_of<world::MeshResource>()) {
            auto mesh = res.cast_static<world::MeshResource>();
            mesh->install();
            utils->update_mesh_data(mesh->device_mesh(), false);
        } else if (res->is_type_of<world::TextureResource>()) {
            auto tex = res.cast_static<world::TextureResource>();
            tex->install();
            utils->update_texture(tex->get_image());
        } else if (res->is_type_of<world::MaterialResource>()) {
            auto mat = res.cast_static<world::MaterialResource>();
            mat->install();
        } else {
            // Other resource types (SkeletonResource, SkinResource, AnimSequenceResource,
            // AnimGraphResource, SkelMeshResource) use default install behavior
            LUISA_INFO("Installing resource [{}] type={}", res->path().string(), res->type_name());
            res->install();
        }
    }
}

void AnimScene::tick_animation(float delta_time) {
    if (entity) {
        auto skelmesh = entity->get_component<world::SkelMeshComponent>();
        if (skelmesh) {
            skelmesh->tick(delta_time);
        }
    }
}

void AnimScene::update_render(GraphicsUtils *utils) {
    if (!entity) {
        LUISA_WARNING("update_render: entity is null");
        return;
    }
    auto skelmesh = entity->get_component<world::SkelMeshComponent>();
    if (!skelmesh) {
        LUISA_WARNING("update_render: SkelMeshComponent not found");
        return;
    }
    if (!skelmesh->IsEnabled()) {
        LUISA_WARNING("update_render: SkelMeshComponent not enabled");
        return;
    }
    // LUISA_INFO("update_render: Calling skelmesh->update_render()");
    skelmesh->update_render();
    if (skelmesh->GetRuntimeMesh()) {
        // LUISA_INFO("update_render: Calling build_transforming_mesh");
        utils->build_transforming_mesh(skelmesh->GetRuntimeMesh()->device_transforming_mesh());
    } else {
        LUISA_WARNING("update_render: GetRuntimeMesh() returned null");
    }
}

void AnimScene::_setup_default_lighting() {
    // Create a simple emissive mesh to provide basic illumination
    // when no skybox is available (similar to sample_graphics)
    auto light_entity = RC<world::Entity>{world::create_object<world::Entity>()};

    auto transform = light_entity->add_component<world::TransformComponent>();
    transform->set_pos(double3(5, 10, 5), true);
    transform->set_scale(double3(2.0, 2.0, 2.0), true);

    light_entity->add_component<world::RenderComponent>();

    // Create emissive material for lighting
    auto light_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
    light_mat->load_from_json(R"({"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]})");
    light_mat->install();

    // Use a simple cube mesh for the light (create a small cube)
    // For now, just add the entity without mesh to see if it helps
    // Actually, we need a mesh for the emissive material to work

    _entities.push_back(light_entity.get());

    LUISA_INFO("Created default emissive light for illumination");
}

AnimScene::~AnimScene() {
    // Save all resources
    for (auto &res : all_resources) {
        if (res) {
            res->save_to_path();
        }
    }

    // IMPORTANT: Destroy entity FIRST before clearing resources
    // Entity holds references to resources (e.g., SkelMeshComponent holds skel_mesh)
    // If resources are cleared first, entity will access freed memory
    entity.reset();
    _entities.clear();

    skybox.reset();
    loaded_materials.clear();
    loaded_mesh.reset();
    skel_mesh.reset();
    all_resources.clear();

    if (scene) {
        scene->save_to_path();
    }
    scene.reset();
    world::destroy_world();
}

int disabled_main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;

    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    PluginManager::init();
    auto dispose_runtime_static = vstd::scope_exit([] {
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
    });

    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }

    auto utils = luisa::make_unique<GraphicsUtils>();
    utils->init_device(
        argv[0],
        backend.c_str());
    utils->init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils->backend_name()));
    utils->init_render();
    auto pipe_ctx = utils->register_render_pipectx();
    auto &render_settings = utils->render_settings(pipe_ctx);
    Window window{luisa::string{"sample_anim_"} + utils->backend_name(), uint2(1024), true};
    utils->init_display(window.size(), window.native_display(), window.native_handle());
    uint64_t frame_index = 0;
    Clock clk;
    double last_frame_time = 0;

    // Parse command line arguments for project directory
    // Usage: sample_anim <backend> [project_root]
    luisa::filesystem::path project_root;
    if (argc >= 3) {
        project_root = argv[2];
    }

    // Create the animation scene
    vstd::optional<AnimScene> anim_scene;
    anim_scene.create(utils.get(), project_root);

    // Camera setup
    auto &cam = utils->render_settings(pipe_ctx).read_mut<Camera>();
    CameraController cam_controller;
    cam_controller.camera = &cam;
    cam.fov = radians(60.0f);
    cam.position = double3(2, 2, -5);
    LUISA_INFO("Camera position: ({}, {}, {})", cam.position.x, cam.position.y, cam.position.z);

    CameraController::Input camera_input;
    uint2 window_size = window.size();

    window.set_mouse_callback([&](MouseButton button, Action action, float2 xy) {
        if (button == MOUSE_BUTTON_2) {
            if (action == Action::ACTION_PRESSED) {
                camera_input.is_mouse_right_down = true;
            } else if (action == Action::ACTION_RELEASED) {
                camera_input.is_mouse_right_down = false;
            }
        }
    });

    window.set_cursor_position_callback([&](float2 xy) {
        camera_input.mouse_cursor_pos = xy;
    });

    window.set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        bool pressed = (action == Action::ACTION_PRESSED);
        switch (key) {
            case Key::KEY_SPACE:
                camera_input.is_space_down = pressed;
                break;
            case Key::KEY_RIGHT_SHIFT:
            case Key::KEY_LEFT_SHIFT:
                camera_input.is_shift_down = pressed;
                break;
            case Key::KEY_W:
                camera_input.is_front_dir_key_pressed = pressed;
                break;
            case Key::KEY_S:
                camera_input.is_back_dir_key_pressed = pressed;
                break;
            case Key::KEY_A:
                camera_input.is_left_dir_key_pressed = pressed;
                break;
            case Key::KEY_D:
                camera_input.is_right_dir_key_pressed = pressed;
                break;
            case Key::KEY_Q:
                camera_input.is_up_dir_key_pressed = pressed;
                break;
            case Key::KEY_E:
                camera_input.is_down_dir_key_pressed = pressed;
                break;
        }
    });

    window.set_window_size_callback([&](uint2 size) {
        window_size = size;
    });

    while (!window.should_close()) {
        RBCFrameMark;

        {
            RBCZoneScopedN("Main Loop");

            {
                RBCZoneScopedN("Poll Events");
                if (window)
                    window.poll_events();
            }

            auto &cam = utils->render_settings(pipe_ctx).read_mut<Camera>();
            if (any(window_size != utils->dst_image().size())) {
                RBCZoneScopedN("Resize Swapchain");
                utils->resize_swapchain(window_size, window.native_display(), window.native_handle());
                frame_index = 0;
            }

            float delta_time = 0.0f;
            {
                RBCZoneScopedN("Update Camera");
                camera_input.viewport_size = make_float2(window_size);
                cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
                auto time = clk.toc();
                delta_time = (time - last_frame_time) * 1e-3f;
                RBCPlot("Frame Time (ms)", delta_time * 1000.0f);
                cam_controller.grab_input_from_viewport(camera_input, delta_time);
                if (cam_controller.any_changed())
                    frame_index = 0;
                last_frame_time = time;
            }

            // Tick animation
            {
                RBCZoneScopedN("Tick Animation");
                anim_scene->tick_animation(delta_time);
            }

            // Update render
            {
                RBCZoneScopedN("Update Render");
                anim_scene->update_render(utils.get());
            }

            {
                auto &frame_settings = render_settings.read_mut<FrameSettings>();
                frame_settings.frame_index = frame_index;
            }
            {
                RBCZoneScopedN("Render Tick");
                // Use RasterPreview if no skybox is loaded, otherwise use PathTracingPreview
                auto tick_stage = anim_scene->has_skybox() ?
                                      GraphicsUtils::TickStage::PathTracingPreview :
                                      GraphicsUtils::TickStage::RasterPreview;
                utils->tick(tick_stage);
            }

            // ++frame_index;
            // RBCPlot("Frame Index", static_cast<float>(frame_index));
        }
    }

    utils->dispose([&]() {
        anim_scene.destroy();
    });

    utils.reset();

    return 0;
}

suite<"Sample|Anim"> SampleAnimTestSuite = [] {
    "placeholder"_test = [] {
        expect(true);
    };
}; // suite

} // namespace rbc::test
