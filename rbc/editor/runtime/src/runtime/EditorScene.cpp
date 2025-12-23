#include "RBCEditorRuntime/runtime/EditorScene.h"
#include <rbc_graphics/mesh_builder.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>
#include <QDebug>
#include <iostream>

namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
}// namespace rbc

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

class MeshLoader {
public:
    [[nodiscard]] static bool can_load(const std::string &path) {
        return path.ends_with(".rbm") ||
               path.ends_with(".obj");
    }
    luisa::shared_ptr<void> load(const std::string &path,
                                 const std::string &options_json) {
        if (path.ends_with(".rbm")) {
            return load_rbm(path);
        } else if (path.ends_with(".obj")) {
            return load_obj(path);
        }
        return nullptr;
    }


private:
    luisa::shared_ptr<Mesh> load_rbm(const std::string &path) {
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
    luisa::shared_ptr<Mesh> load_obj(const std::string &path) {
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
};

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

    initMaterial();

    qDebug("EditorScene initialized (light will be initialized on first entity)");
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

    // Remove light (only if it was initialized)
    if (light_initialized_ && light_id_ != 0) {
        lights_->remove_area_light(light_id_);
    }
    if (lights_) {
        lights_.destroy();
    }

    // Discard material
    sm.mat_manager().discard_mat_instance(default_mat_code_);
}

void EditorScene::initMaterial() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();

    material::OpenPBR mat{};
    mat.base.albedo = {0.5f, 0.5f, 0.5f};

    // Make material instance
    default_mat_code_ = sm.mat_manager().emplace_mat_instance(
        mat,
        cmdlist,
        sm.bindless_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue(),
        material::PolymorphicMaterial::index<material::OpenPBR>);
}

void EditorScene::initLight() {
    using namespace luisa;
    using namespace luisa::compute;
    lights_.create();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();

    luisa::float3 light_pos(0.5f, 0.5f, 1.0f);
    float4x4 area_light_transform =
        translation(light_pos) *
        rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f) * scaling(0.1f);

    light_id_ = lights_->add_area_light(cmdlist, area_light_transform, light_emission);
    light_initialized_ = true;
    qDebug() << "EditorScene: Light initialized";
}

void EditorScene::updateFromSync(const SceneSync &sync) {
    using namespace luisa;
    using namespace luisa::compute;

    const auto &entities = sync.entities();
    const auto &resources = sync.resources();

    qDebug() << "EditorScene: Syncing " << entities.size() << " entities, {} resources" << resources.size();

    // Build resource path map
    luisa::unordered_map<int, luisa::string> resource_paths;
    for (const auto &res : resources) {
        resource_paths[res.id] = res.path;
        qDebug() << "  Resource " << res.id << ":" << res.path;
    }

    // Track which entities we've seen in this update
    luisa::unordered_set<int> seen_entities;

    // Update or add entities
    for (const auto &entity : entities) {
        // qDebug("  Entity {}: {}, has_render={}", entity.id, entity.name, entity.has_render_component);

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
            // Update existing entity transform
            qDebug() << "EditorScene: Updating entity" << entity.id << "transform";
            updateEntityTransform(entity.id, entity.transform);
        } else {
            // Add new entity
            qDebug() << "EditorScene: Adding new entity" << entity.id << "with mesh" << QString::fromUtf8(mesh_path.c_str());
            addEntity(entity.id, mesh_path, entity.transform);
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

    tlas_ready_ = !instances_.empty();
    qDebug() << "EditorScene: TLAS ready = " << tlas_ready_ << ":" << instances_.size() << " instances";
}

void EditorScene::addEntity(int entity_id, const luisa::string &mesh_path,
                            const Transform &transform) {
    using namespace luisa;
    using namespace luisa::compute;

    qDebug() << "Adding entity " << entity_id << "  with mesh: " << mesh_path;

    EntityInstance instance;
    instance.entity_id = entity_id;
    instance.mesh_path = mesh_path;

    // Load mesh from file
    instance.device_mesh = loadMeshFromFile(mesh_path);
    if (!instance.device_mesh) {
        LUISA_ERROR("Failed to load mesh: {}", mesh_path);
        return;
    }

    // Wait for mesh to be loaded
    instance.device_mesh->wait_finished();
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

    qDebug() << "Entity " << entity_id << " added to TLAS at index " << instance.tlas_index;
}

void EditorScene::setAnimationTransform(int entity_id, const Transform &transform) {
    animation_transforms_[entity_id] = transform;

    // Also update the entity's actual transform
    updateEntityTransform(entity_id, transform);
}

void EditorScene::clearAnimationTransforms() {
    animation_transforms_.clear();
}

void EditorScene::updateEntityTransform(int entity_id, const Transform &transform) {
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

void EditorScene::removeEntity(int entity_id) {
    using namespace luisa;
    using namespace luisa::compute;

    auto it = entity_map_.find(entity_id);
    if (it == entity_map_.end()) {
        return;
    }

    qDebug() << "Removing entity " << entity_id;

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

RC<DeviceMesh> EditorScene::loadMeshFromFile(const luisa::string &path) {
    using namespace luisa;
    using namespace luisa::compute;

    qDebug() << "Loading mesh from file: " << path.c_str();

    // Use MeshLoader to load the mesh
    MeshLoader loader;
    if (!loader.can_load(path.c_str())) {
        LUISA_ERROR("MeshLoader cannot load file: {}", path);
        return RC<DeviceMesh>{};
    }

    auto mesh_ptr = loader.load(path.c_str(), "{}");

    auto mesh = luisa::static_pointer_cast<rbc::Mesh>(mesh_ptr);
    if (!mesh) {
        LUISA_ERROR("Failed to load mesh: {}", path);
        return RC<DeviceMesh>{};
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
    convertMeshToBuilder(mesh, mesh_builder);

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
        mesh_builder.indices_count() / 3,
        mesh_builder.contained_normal(),
        mesh_builder.contained_tangent(),
        mesh_builder.uv_count(),
        std::move(submesh_triangle_offset),
        false,// No need to build BLAS for origin mesh
        true);

    qDebug() << "Mesh loaded: " << mesh->vertices.size() << " vertices, " << mesh->indices.size() << " indices";

    return device_mesh;
}

void EditorScene::convertMeshToBuilder(const luisa::shared_ptr<rbc::Mesh> &mesh,
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
    }
    // Copy indices
    auto &triangle = builder.triangle_indices.emplace_back();
    for (uint32_t idx : mesh->indices) {
        triangle.push_back(idx);
    }
}

int EditorScene::getEntityIdFromInstanceId(uint32_t instance_id) const {
    // Search through instances to find the one with matching tlas_index
    for (const auto &instance : instances_) {
        if (instance.tlas_index == instance_id) {
            return instance.entity_id;
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

}// namespace rbc