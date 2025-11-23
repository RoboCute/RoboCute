#include <rbc_runtime/scene.h>
#include <luisa/core/logging.h>
#include <cstring>

namespace rbc {

// TransformComponent implementation

void TransformComponent::compute_local_matrix(luisa::float4x4 &out_matrix) const {
    // Build TRS matrix: Translation * Rotation * Scale

    // Normalize quaternion
    luisa::float4 q = rotation;
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 0.0001f) {
        q.x /= len;
        q.y /= len;
        q.z /= len;
        q.w /= len;
    }

    // Convert quaternion to rotation matrix
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    // Build matrix with TRS composition
    out_matrix[0][0] = (1.f - 2.f * (yy + zz)) * scaling.x;
    out_matrix[0][1] = (2.f * (xy + wz)) * scaling.x;
    out_matrix[0][2] = (2.f * (xz - wy)) * scaling.x;
    out_matrix[0][3] = 0.f;

    out_matrix[1][0] = (2.f * (xy - wz)) * scaling.y;
    out_matrix[1][1] = (1.f - 2.f * (xx + zz)) * scaling.y;
    out_matrix[1][2] = (2.f * (yz + wx)) * scaling.y;
    out_matrix[1][3] = 0.f;

    out_matrix[2][0] = (2.f * (xz + wy)) * scaling.z;
    out_matrix[2][1] = (2.f * (yz - wx)) * scaling.z;
    out_matrix[2][2] = (1.f - 2.f * (xx + yy)) * scaling.z;
    out_matrix[2][3] = 0.f;

    out_matrix[3][0] = position.x;
    out_matrix[3][1] = position.y;
    out_matrix[3][2] = position.z;
    out_matrix[3][3] = 1.f;
}

void TransformComponent::mark_dirty() {
    global_dirty = true;
}

luisa::unique_ptr<IComponent> TransformComponent::clone() const {
    auto cloned = luisa::make_unique<TransformComponent>();
    cloned->position = position;
    cloned->rotation = rotation;
    cloned->scaling = scaling;
    cloned->parent = parent;
    cloned->children = children;
    cloned->global_transform = global_transform;
    cloned->global_dirty = global_dirty;
    return cloned;
}

// void TransformComponent::serialize(Serializer<IComponent> &serializer) const {
    // Serialize using _store method which handles different types automatically
    // serializer._store(position, "position");
    // serializer._store(rotation, "rotation");
    // serializer._store(scaling, "scaling");
    // serializer._store(parent, "parent");
    // serializer._store(children, "children");
// }

// void TransformComponent::deserialize(DeSerializer<IComponent> &deserializer) {
    // Deserialize using _load method
    // deserializer._load(position, "position");
    // deserializer._load(rotation, "rotation");
    // deserializer._load(scaling, "scaling");
    // deserializer._load(parent, "parent");
    // deserializer._load(children, "children");

    // Mark as dirty after deserialization
//     global_dirty = true;
// }

// TransformSystem implementation

void TransformSystem::update(float delta_time) {
    if (!entity_manager_) return;

    auto entities = entity_manager_->get_entities_with_component<TransformComponent>();
    if (entities.empty()) return;

    // Organize entities by hierarchy depth
    luisa::vector<luisa::vector<EntityID>> layers;
    organize_by_depth(entities, layers);

    // Update each layer sequentially (parents before children)
    for (const auto &layer : layers) {
        for (EntityID entity : layer) {
            update_single_transform(entity);
        }
    }
}

void TransformSystem::update_transforms(luisa::span<const EntityID> entities) {
    for (EntityID entity : entities) {
        update_single_transform(entity);
    }
}

void TransformSystem::organize_by_depth(
    const luisa::vector<EntityID> &entities,
    luisa::vector<luisa::vector<EntityID>> &layers) {

    layers.clear();

    // Calculate depth for each entity
    luisa::unordered_map<EntityID, size_t> entity_depths;

    for (EntityID entity : entities) {
        size_t depth = 0;
        EntityID current = entity;

        // Traverse up the hierarchy to find depth
        while (current != INVALID_ENTITY) {
            auto *transform = entity_manager_->get_component<TransformComponent>(current);
            if (!transform || transform->parent == INVALID_ENTITY) {
                break;
            }
            current = transform->parent;
            ++depth;

            // Prevent infinite loops
            if (depth > 1000) {
                LUISA_WARNING("Detected potential cycle in transform hierarchy for entity {}", entity);
                depth = 0;
                break;
            }
        }

        entity_depths[entity] = depth;

        // Ensure layers vector is large enough
        if (depth >= layers.size()) {
            layers.resize(depth + 1);
        }
        layers[depth].push_back(entity);
    }
}

void TransformSystem::update_single_transform(EntityID entity) {
    auto *transform = entity_manager_->get_component<TransformComponent>(entity);
    if (!transform) return;

    // Compute local transform matrix
    luisa::float4x4 local_matrix;
    transform->compute_local_matrix(local_matrix);

    // If has parent, combine with parent's global transform
    if (transform->parent != INVALID_ENTITY) {
        auto *parent_transform = entity_manager_->get_component<TransformComponent>(transform->parent);
        if (parent_transform) {
            // Check if parent is dirty
            if (parent_transform->global_dirty) {
                // Parent should have been updated already in previous layer
                // but mark this transform as dirty to ensure update
                transform->global_dirty = true;
            }

            if (transform->global_dirty) {
                // Multiply: global = parent_global * local
                transform->global_transform = parent_transform->global_transform * local_matrix;
                transform->global_dirty = false;
            }
        } else {
            // Parent doesn't exist, treat as root
            transform->global_transform = local_matrix;
            transform->global_dirty = false;
        }
    } else {
        // Root entity, global = local
        if (transform->global_dirty) {
            transform->global_transform = local_matrix;
            transform->global_dirty = false;
        }
    }
}

}// namespace rbc
