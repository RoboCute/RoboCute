#include "editor_scene.h"
#include <rbc_graphics/mesh_builder.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>

namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
}// namespace rbc

namespace rbc {

static const luisa::float3 light_emission{luisa::float3(7, 6, 3) * 1000.f};

EditorScene::EditorScene() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();

    // Initialize material types
    sm.mat_manager().emplace_mat_type<material::OpenPBR>(
        sm.bindless_allocator(), 65536,
        material::PolymorphicMaterial::index<material::OpenPBR>);
    sm.mat_manager().emplace_mat_type<material::Unlit>(
        sm.bindless_allocator(), 65536,
        material::PolymorphicMaterial::index<material::Unlit>);

    init_material();
    init_light();

    LUISA_INFO("EditorScene initialized");
}

EditorScene::~EditorScene() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();

    // Remove all mesh instances from TLAS
    for (auto &instance : instances_) {
        if (instance.mesh_loaded && instance.tlas_index != 0) {
            sm.accel_manager().remove_mesh_instance(
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                instance.tlas_index);
        }
    }

    // Remove light
    if (light_id_ != 0) {
        lights_.remove_area_light(light_id_);
    }

    // Discard material
    sm.mat_manager().discard_mat_instance(default_mat_code_);
}

void EditorScene::init_material() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();

    material::OpenPBR mat{};
    mat.base.albedo = make_half3((half)0.5f);

    // Make material instance
    default_mat_code_ = sm.mat_manager().emplace_mat_instance(
        mat,
        cmdlist,
        sm.bindless_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue(),
        material::PolymorphicMaterial::index<material::OpenPBR>);
}

void EditorScene::init_light() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();

    luisa::float3 light_pos(0.5f, 0.5f, 1.0f);
    float4x4 area_light_transform =
        translation(light_pos) *
        rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f) * scaling(0.1f);

    light_id_ = lights_.add_area_light(cmdlist, area_light_transform, light_emission);
}

void EditorScene::update_from_sync(const SceneSync &sync) {
    using namespace luisa;
    using namespace luisa::compute;

    const auto &entities = sync.entities();
    const auto &resources = sync.resources();

    LUISA_INFO("EditorScene: Syncing {} entities, {} resources", entities.size(), resources.size());

    // Build resource path map
    luisa::unordered_map<int, luisa::string> resource_paths;
    for (const auto &res : resources) {
        resource_paths[res.id] = res.path;
        LUISA_INFO("  Resource {}: {}", res.id, res.path);
    }

    // Track which entities we've seen in this update
    luisa::unordered_set<int> seen_entities;

    // Update or add entities
    for (const auto &entity : entities) {
        LUISA_INFO("  Entity {}: {}, has_render={}", entity.id, entity.name, entity.has_render_component);

        if (!entity.has_render_component) {
            continue;
        }

        seen_entities.insert(entity.id);

        // Get mesh path from resource
        auto path_it = resource_paths.find(entity.render_component.mesh_id);
        if (path_it == resource_paths.end()) {
            LUISA_WARNING("Entity {} references unknown mesh resource {}",
                          entity.id, entity.render_component.mesh_id);
            continue;
        }

        const luisa::string &mesh_path = path_it->second;

        // Check if entity already exists
        auto it = entity_map_.find(entity.id);
        if (it != entity_map_.end()) {
            // Update existing entity
            LUISA_INFO("  Updating entity {} transform", entity.id);
            update_entity_transform(entity.id, entity.transform);
        } else {
            // Add new entity
            LUISA_INFO("  Adding new entity {} with mesh {}", entity.id, mesh_path);
            add_entity(entity.id, mesh_path, entity.transform);
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
        remove_entity(entity_id);
    }

    tlas_ready_ = !instances_.empty();
    LUISA_INFO("EditorScene: TLAS ready = {}, {} instances", tlas_ready_, instances_.size());
}

void EditorScene::add_entity(int entity_id, const luisa::string &mesh_path,
                             const Transform &transform) {
    using namespace luisa;
    using namespace luisa::compute;

    LUISA_INFO("Adding entity {} with mesh: {}", entity_id, mesh_path);

    EntityInstance instance;
    instance.entity_id = entity_id;
    instance.mesh_path = mesh_path;

    // Load mesh from file
    instance.device_mesh = load_mesh_from_file(mesh_path);
    if (!instance.device_mesh) {
        LUISA_ERROR("Failed to load mesh: {}", mesh_path);
        return;
    }

    // Wait for mesh to be loaded
    instance.device_mesh->sync_wait();
    instance.mesh_loaded = true;

    // Add to TLAS
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();

    float4x4 transform_matrix =
        translation(transform.position) *
        // TODO: Apply rotation quaternion
        scaling(transform.scale);

    instance.tlas_index = sm.accel_manager().emplace_mesh_instance(
        cmdlist, sm.host_upload_buffer(),
        sm.buffer_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue(),
        instance.device_mesh->mesh_data(),
        {&default_mat_code_, 1},
        transform_matrix);

    // Add to instances
    size_t idx = instances_.size();
    instances_.push_back(instance);
    entity_map_[entity_id] = idx;

    LUISA_INFO("Entity {} added to TLAS at index {}", entity_id, instance.tlas_index);
}

void EditorScene::update_entity_transform(int entity_id, const Transform &transform) {
    using namespace luisa;
    using namespace luisa::compute;

    auto it = entity_map_.find(entity_id);
    if (it == entity_map_.end()) {
        return;
    }

    EntityInstance &instance = instances_[it->second];
    if (!instance.mesh_loaded) {
        return;
    }

    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();

    float4x4 transform_matrix =
        translation(transform.position) *
        // TODO: Apply rotation quaternion
        scaling(transform.scale);

    sm.accel_manager().set_mesh_instance(
        render_device.lc_main_cmd_list(),
        sm.buffer_uploader(),
        instance.tlas_index,
        transform_matrix, ~0ull, true);
}

void EditorScene::remove_entity(int entity_id) {
    using namespace luisa;
    using namespace luisa::compute;

    auto it = entity_map_.find(entity_id);
    if (it == entity_map_.end()) {
        return;
    }

    LUISA_INFO("Removing entity {}", entity_id);

    EntityInstance &instance = instances_[it->second];

    // Remove from TLAS
    if (instance.mesh_loaded && instance.tlas_index != 0) {
        auto &sm = SceneManager::instance();
        sm.accel_manager().remove_mesh_instance(
            sm.buffer_allocator(),
            sm.buffer_uploader(),
            instance.tlas_index);
    }

    // Remove from instances (swap with last)
    size_t idx = it->second;
    if (idx < instances_.size() - 1) {
        instances_[idx] = instances_.back();
        entity_map_[instances_[idx].entity_id] = idx;
    }
    instances_.pop_back();
    entity_map_.erase(entity_id);
}

RC<DeviceMesh> EditorScene::load_mesh_from_file(const luisa::string &path) {
    using namespace luisa;
    using namespace luisa::compute;

    LUISA_INFO("Loading mesh from file: {}", path);

    // Use MeshLoader to load the mesh
    MeshLoader loader;
    if (!loader.can_load(path.c_str())) {
        LUISA_ERROR("MeshLoader cannot load file: {}", path);
        return nullptr;
    }

    auto mesh_ptr = loader.load(path.c_str(), "{}");
    auto mesh = luisa::static_pointer_cast<rbc::Mesh>(mesh_ptr);
    if (!mesh) {
        LUISA_ERROR("Failed to load mesh: {}", path);
        return nullptr;
    }

    // Compute normals if not present
    bool has_normals = false;
    for (const auto &v : mesh->vertices) {
        if (v.normal[0] != 0.0f || v.normal[1] != 0.0f || v.normal[2] != 0.0f) {
            has_normals = true;
            break;
        }
    }
    if (!has_normals) {
        mesh->compute_normals();
    }

    // Convert to MeshBuilder format
    MeshBuilder mesh_builder;
    convert_mesh_to_builder(mesh, mesh_builder);

    // Create DeviceMesh
    RC<DeviceMesh> device_mesh{new DeviceMesh{}};
    luisa::vector<std::byte> mesh_data;
    vstd::vector<uint> submesh_triangle_offset;
    mesh_builder.write_to(mesh_data, submesh_triangle_offset);

    BinaryBlob blob{
        mesh_data.data(),
        mesh_data.size_bytes(),
        [mesh_data = std::move(mesh_data)](void *ptr) {}};

    device_mesh->async_load_from_memory(
        std::move(blob),
        mesh_builder.vertex_count(),
        mesh_builder.contained_normal(),
        mesh_builder.contained_tangent(),
        mesh_builder.uv_count(),
        std::move(submesh_triangle_offset),
        false,// No need to build BLAS for origin mesh
        true);

    LUISA_INFO("Mesh loaded: {} vertices, {} indices",
               mesh->vertices.size(), mesh->indices.size());

    return device_mesh;
}

void EditorScene::convert_mesh_to_builder(const luisa::shared_ptr<rbc::Mesh> &mesh,
                                          MeshBuilder &builder) {
    using namespace luisa;

    // Create UV layer 0
    if (!mesh->vertices.empty()) {
        builder.uvs.emplace_back();
    }

    // Copy vertices
    for (const auto &v : mesh->vertices) {
        builder.position.push_back(float3(v.position[0], v.position[1], v.position[2]));
        builder.normal.push_back(float3(v.normal[0], v.normal[1], v.normal[2]));
        builder.uvs[0].push_back(float2(v.texcoord[0], v.texcoord[1]));
        // Note: tangent handling could be added if needed
    }

    // Copy indices
    auto &triangle = builder.triangle_indices.emplace_back();
    for (uint32_t idx : mesh->indices) {
        triangle.push_back(idx);
    }
}

}// namespace rbc
