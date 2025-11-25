#pragma once

#include "rbc_world/resource.h"
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <list>

namespace rbc {

/**
 * ResourceStorage
 * * Memory pool management
 * * LRU cache strategy
 * * Thread-safe resource access
 */
class ResourceStorage {
public:
    explicit ResourceStorage(size_t budget_bytes);
    ~ResourceStorage() = default;

    // === Storage/Retrieval ===
    void store(ResourceID id, std::shared_ptr<void> data,
               ResourceType type, size_t size);
    std::shared_ptr<void> get(ResourceID id);
    bool contains(ResourceID id) const;

    // === State Query ===
    ResourceState get_state(ResourceID id) const;
    void set_state(ResourceID id, ResourceState state);
    size_t get_size(ResourceID id) const;

    // === Removal ===
    void remove(ResourceID id);
    void clear();

    // === Cache Management ===
    size_t get_memory_usage() const { return current_memory_usage_.load(); }
    size_t get_budget() const { return budget_bytes_; }
    void set_budget(size_t bytes);

    // LRU eviction
    void evict_lru();
    void evict_until_budget();

    // Access update (for LRU)
    void touch(ResourceID id);

private:
    struct ResourceEntry {
        ResourceID id;
        std::shared_ptr<void> data;
        ResourceType type;
        ResourceState state;
        size_t size;

        // LRU list iterator
        std::list<ResourceID>::iterator lru_iter;
    };

    std::unordered_map<ResourceID, ResourceEntry> entries_;
    std::list<ResourceID> lru_list_;// Most recently used at front

    size_t budget_bytes_;
    std::atomic<size_t> current_memory_usage_{0};

    mutable std::shared_mutex mutex_;

    // Helper methods
    void evict_resource_internal(ResourceID id);
};

}// namespace rbc
