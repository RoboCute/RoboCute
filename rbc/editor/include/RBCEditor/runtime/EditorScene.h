#pragma once
/**
The Editor-end Scene Management
==========================================
- Entity-Component Hierarchy
- Proxy Resource for Visualization
- Sync with Network backend
*/

#include "RBCEditor/runtime/HttpClient.h"

namespace rbc {

// The Editor-Side Scene Data
struct EditorScene {
};

struct EditorSceneEntity {
    int id = 0;
    luisa::string name;
    // TransformComponent
    // RenderComponent
};

// Resource Metadata
struct EditorResourceMetadata {
    uint64_t id;
    int type = 0;
    luisa::string path;
    int state = 0;
    size_t size_bytes;
};

class SceneSyncManager {
public:
    SceneSyncManager();
    ~SceneSyncManager();

    // Optional Additive Update From Server
    bool UpdateFromServer(const HttpClient &client, bool is_additive = true);
private:
    luisa::vector<EditorSceneEntity> entities_;
    luisa::vector<EditorResourceMetadata> resources_;
    luisa::unordered_map<int, unsigned int> entity_map_;       // entity_id -> entity local index
    luisa::unordered_map<uint64_t, unsigned int> resource_map_;// resource_id -> resource local index
    bool has_changed_;

    // helper functions
};

}// namespace rbc