#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include <rbc_world/base_object.h>
#include <rbc_world/texture_loader.h>
#include <rbc_world/importers/texture_importer_exr.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_app/graphics_utils.h>
#include <luisa/core/logging.h>
#include <QDebug>

namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
}// namespace rbc

namespace rbc {

EditorScene::EditorScene() {
    qDebug("EditorScene: Initializing with Runtime Entity-Component system");
    initWorld();
    initMaterial();
    initSkybox();
    qDebug("EditorScene: Initialized successfully");
}

EditorScene::~EditorScene() {
    qDebug("EditorScene: Destroying");

    // Clear all entities first
    for (auto &info : entities_) {
        if (info.entity) {
            info.entity->delete_this();
        }
    }
    entities_.clear();
    entity_map_.clear();

    // Clear mesh cache
    mesh_cache_.clear();

    // Clear material
    default_material_.reset();

    // Clear skybox
    default_skybox_.reset();

    // Destroy world system
    if (world_initialized_) {
        world::destroy_world();
        world_initialized_ = false;
    }

    qDebug("EditorScene: Destroyed");
}

void EditorScene::initWorld() {
    // Initialize the world system with a temporary directory for Editor
    // The Editor doesn't save/load scene files, so we use a temp path
    auto &render_device = RenderDevice::instance();
    auto runtime_dir = render_device.lc_ctx().runtime_directory();
    luisa::filesystem::path editor_scene_dir = runtime_dir / "editor_scene";

    if (!luisa::filesystem::exists(editor_scene_dir)) {
        luisa::filesystem::create_directories(editor_scene_dir);
    }

    world::init_world(editor_scene_dir);
    world::load_all_resources_from_meta();
    world_initialized_ = true;
    auto path_str = luisa::to_string(editor_scene_dir);
    qDebug() << "EditorScene: World initialized at" << QString::fromUtf8(path_str.c_str(), static_cast<int>(path_str.size()));
}

void EditorScene::initMaterial() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();

    // Register material types with SceneManager (required before any MaterialResource can be used)
    sm.mat_manager().emplace_mat_type<material::OpenPBR>(
        sm.bindless_allocator(), 65536,
        material::PolymorphicMaterial::index<material::OpenPBR>);
    sm.mat_manager().emplace_mat_type<material::Unlit>(
        sm.bindless_allocator(), 65536,
        material::PolymorphicMaterial::index<material::Unlit>);

    // Create a default PBR material for all entities
    default_material_ = world::create_object<world::MaterialResource>();

    // Use a simple gray PBR material
    auto mat_json = R"({"type": "pbr", "specular_roughness": 0.5, "weight_metallic": 0.0, "base_albedo": [0.5, 0.5, 0.5]})"sv;
    default_material_->load_from_json(mat_json);

    qDebug("EditorScene: Default material created");
}

void EditorScene::initSkybox() {
    using namespace luisa;

    qDebug("EditorScene: Loading default skybox from sky.exr");

    // Create texture loader and importer
    world::TextureLoader tex_loader;
    world::ExrTextureImporter exr_importer;

    // Create skybox texture resource
    default_skybox_ = world::create_object<world::TextureResource>();

    // Import sky.exr (assumes it exists)
    if (!exr_importer.import(default_skybox_, &tex_loader, "sky.exr", 1, false)) {
        LUISA_ERROR("Failed to import skybox from sky.exr");
        default_skybox_.reset();
        return;
    }

    // Finish texture loading tasks
    tex_loader.finish_task();

    // Initialize device resource
    default_skybox_->init_device_resource();

    // Update skybox to render plugin (requires GraphicsUtils)
    auto *appBase = EditorEngine::instance().getRenderAppBase();
    if (appBase && default_skybox_->get_image()) {
        // Update skybox in render plugin first
        RC<DeviceImage> image{default_skybox_->get_image()};
        appBase->utils.render_plugin()->update_skybox(image);

        // Update texture data to GPU through render thread
        appBase->utils.update_texture(default_skybox_->get_image());

        qDebug("EditorScene: Default skybox loaded and updated to render plugin");
    } else {
        LUISA_WARNING("EditorScene: Cannot update skybox - render app not available");
    }
}

void EditorScene::onFrameTick() {
    // Process pending entities after IO has completed
    processPendingEntities();
}

void EditorScene::updateFromSync(const SceneSync &sync) {
    using namespace luisa;

    const auto &scene_entities = sync.entities();
    const auto &resources = sync.resources();

    qDebug() << "EditorScene: Syncing" << scene_entities.size() << "entities," << resources.size() << "resources";

    // Build resource path map
    luisa::unordered_map<int, luisa::string> resource_paths;
    for (const auto &res : resources) {
        resource_paths[res.id] = res.path;
    }

    // Track which entities we've seen in this update
    luisa::unordered_set<int> seen_entities;

    // Update or add entities
    for (const auto &scene_entity : scene_entities) {
        if (!scene_entity.has_render_component) {
            continue;
        }

        seen_entities.insert(scene_entity.id);

        // Get mesh path from resource
        auto path_it = resource_paths.find(scene_entity.render_component.mesh_id);
        if (path_it == resource_paths.end()) {
            LUISA_WARNING("Entity {} references unknown mesh resource {}",
                          scene_entity.id, scene_entity.render_component.mesh_id);
            continue;
        }

        const luisa::string &mesh_path = path_it->second;

        // Check if entity already exists
        auto it = entity_map_.find(scene_entity.id);
        if (it != entity_map_.end()) {
            // Update existing entity transform
            updateEntityTransform(scene_entity.id, scene_entity.transform);
        } else {
            // Add new entity
            qDebug() << "EditorScene: Adding new entity" << scene_entity.id
                     << "with mesh" << QString::fromUtf8(mesh_path.c_str());
            addEntity(scene_entity.id, mesh_path, scene_entity.transform);
        }
    }

    // Remove entities that are no longer in the scene
    luisa::vector<int> to_remove;
    for (const auto &[entity_id, idx] : entity_map_) {
        if (seen_entities.find(entity_id) == seen_entities.end()) {
            to_remove.push_back(entity_id);
        }
    }
    for (int entity_id : to_remove) {
        removeEntity(entity_id);
    }

    scene_ready_ = !entities_.empty();
    qDebug() << "EditorScene: Scene ready =" << scene_ready_ << "with" << entities_.size() << "entities";
}

void EditorScene::addEntity(int entity_id, const luisa::string &mesh_path,
                            const Transform &transform) {
    using namespace luisa;

    qDebug() << "EditorScene: Adding entity" << entity_id << "with mesh:" << mesh_path.c_str();

    // Load mesh resource
    auto mesh_resource = loadMeshResource(mesh_path);
    if (!mesh_resource) {
        LUISA_ERROR("Failed to load mesh resource: {}", mesh_path);
        return;
    }

    // Create entity
    auto *entity = world::create_object<world::Entity>();

    // Add transform component
    auto *tr = entity->add_component<world::TransformComponent>();

    // Convert Transform to proper types and set
    double3 position = make_double3(transform.position);

    // Convert quaternion (x, y, z, w) to Quaternion
    Quaternion rotation{make_float4(transform.rotation.x, transform.rotation.y,
                                    transform.rotation.z, transform.rotation.w)};

    double3 scale = make_double3(transform.scale);

    tr->set_trs(position, rotation, scale, true);

    // Add render component (but don't start update yet - need to wait for mesh IO)
    entity->add_component<world::RenderComponent>();

    // Store entity info
    EntityInfo info;
    info.entity_id = entity_id;
    info.mesh_path = mesh_path;
    info.entity = entity;
    info.mesh_resource = mesh_resource;

    size_t idx = entities_.size();
    entities_.push_back(std::move(info));
    entity_map_[entity_id] = idx;

    // Add to pending list - will call start_update_object after IO completes
    pending_entities_.push_back({entity_id, entity, mesh_resource});

    qDebug() << "EditorScene: Entity" << entity_id << "added to pending (waiting for IO)";
}

void EditorScene::updateEntityTransform(int entity_id, const Transform &transform) {
    using namespace luisa;

    auto it = entity_map_.find(entity_id);
    if (it == entity_map_.end()) {
        return;
    }

    EntityInfo &info = entities_[it->second];
    if (!info.entity) {
        return;
    }

    // Get transform component
    auto *tr = info.entity->get_component<world::TransformComponent>();
    if (!tr) {
        return;
    }

    // Update transform
    double3 position = make_double3(transform.position);

    Quaternion rotation{make_float4(transform.rotation.x, transform.rotation.y,
                                    transform.rotation.z, transform.rotation.w)};

    double3 scale = make_double3(transform.scale);

    tr->set_trs(position, rotation, scale, true);
}

void EditorScene::removeEntity(int entity_id) {
    auto it = entity_map_.find(entity_id);
    if (it == entity_map_.end()) {
        return;
    }

    qDebug() << "EditorScene: Removing entity" << entity_id;

    size_t idx = it->second;
    EntityInfo &info = entities_[idx];

    // Delete the entity (this will clean up components and TLAS entry automatically)
    if (info.entity) {
        info.entity->delete_this();
    }

    // Swap with last and remove
    if (idx < entities_.size() - 1) {
        entities_[idx] = std::move(entities_.back());
        entity_map_[entities_[idx].entity_id] = idx;
    }
    entities_.pop_back();
    entity_map_.erase(entity_id);
}

void EditorScene::setAnimationTransform(int entity_id, const Transform &transform) {
    animation_transforms_[entity_id] = transform;

    // Also update the entity's actual transform
    updateEntityTransform(entity_id, transform);
}

void EditorScene::clearAnimationTransforms() {
    animation_transforms_.clear();
}

RC<world::MeshResource> EditorScene::loadMeshResource(const luisa::string &path) {
    // Check cache first
    auto cache_it = mesh_cache_.find(path);
    if (cache_it != mesh_cache_.end()) {
        return cache_it->second;
    }

    qDebug() << "EditorScene: Loading mesh resource from:" << path.c_str();

    // Create new mesh resource
    auto mesh = world::create_object<world::MeshResource>();

    // Decode mesh from file (supports .obj, .gltf, etc. through registered importers)
    if (!mesh->decode(path)) {
        LUISA_ERROR("Failed to decode mesh from: {}", path);
        return {};
    }

    // Initialize device resource
    mesh->init_device_resource();

    // Upload mesh data to GPU through render thread
    // This is critical - without this, the mesh data won't be visible
    auto *appBase = EditorEngine::instance().getRenderAppBase();
    if (appBase && mesh->device_mesh()) {
        // AppBase provides utils directly, no need to cast to VisApp
        appBase->utils.update_mesh_data(mesh->device_mesh(), false);
        qDebug() << "EditorScene: Mesh data uploaded to GPU";
    }

    // Cache and return
    RC<world::MeshResource> mesh_rc{mesh};
    mesh_cache_[path] = mesh_rc;

    qDebug() << "EditorScene: Mesh resource loaded:" << mesh->vertex_count() << "vertices,"
             << mesh->triangle_count() << "triangles";

    return mesh_rc;
}

int EditorScene::getEntityIdFromInstanceId(uint32_t instance_id) const {
    // Search through entities to find the one with matching tlas_index
    for (const auto &info : entities_) {
        if (!info.entity) continue;

        auto *render = info.entity->get_component<world::RenderComponent>();
        if (render && render->get_tlas_index() == instance_id) {
            return info.entity_id;
        }
    }
    return -1;// Not found
}

luisa::vector<int> EditorScene::getEntityIdsFromInstanceIds(const luisa::vector<uint> &instance_ids) const {
    luisa::vector<int> entity_ids;
    entity_ids.reserve(instance_ids.size());

    for (uint instance_id : instance_ids) {
        int entity_id = getEntityIdFromInstanceId(instance_id);
        if (entity_id >= 0) {
            entity_ids.push_back(entity_id);
        }
    }

    return entity_ids;
}

void EditorScene::processPendingEntities() {
    if (pending_entities_.empty()) {
        return;
    }

    qDebug() << "EditorScene: Processing" << pending_entities_.size() << "pending entities";

    for (auto &pending : pending_entities_) {
        if (!pending.entity) continue;

        auto *render = pending.entity->get_component<world::RenderComponent>();
        if (!render) continue;

        // Now that IO has completed, we can safely start the render update
        // The mesh data is on GPU, so BLAS will be built with valid data
        luisa::span<RC<world::MaterialResource> const> mats;
        if (default_material_) {
            mats = {&default_material_, 1};
        }
        render->update_object(mats, pending.mesh_resource.get());

        qDebug() << "EditorScene: Entity" << pending.entity_id << "render started";
    }

    pending_entities_.clear();
}

}// namespace rbc
