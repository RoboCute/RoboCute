#include "rbc_world/resource_storage.h"

namespace rbc {

ResourceStorage::ResourceStorage(size_t budget_bytes)
    : budget_bytes_(budget_bytes) {}

void ResourceStorage::store(ResourceID id, std::shared_ptr<void> data,
                            ResourceType type, size_t size) {
    std::unique_lock lock(mutex_);

    // Check if already exists
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        // Update existing entry
        current_memory_usage_ -= it->second.size;
        it->second.data = data;
        it->second.size = size;
        it->second.state = ResourceState::Loaded;
        current_memory_usage_ += size;

        // Move to front of LRU list
        lru_list_.erase(it->second.lru_iter);
        lru_list_.push_front(id);
        it->second.lru_iter = lru_list_.begin();
    } else {
        // Create new entry
        ResourceEntry entry;
        entry.id = id;
        entry.data = data;
        entry.type = type;
        entry.state = ResourceState::Loaded;
        entry.size = size;

        // Add to front of LRU list
        lru_list_.push_front(id);
        entry.lru_iter = lru_list_.begin();

        entries_[id] = entry;
        current_memory_usage_ += size;
    }

    // Evict if over budget
    while (current_memory_usage_ > budget_bytes_ && !lru_list_.empty()) {
        evict_lru();
    }
}

std::shared_ptr<void> ResourceStorage::get(ResourceID id) {
    std::shared_lock lock(mutex_);

    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return nullptr;
    }

    // Touch for LRU (need to upgrade to unique lock)
    lock.unlock();
    touch(id);

    // Reacquire shared lock
    lock.lock();
    it = entries_.find(id);
    if (it == entries_.end()) {
        return nullptr;
    }

    return it->second.data;
}

bool ResourceStorage::contains(ResourceID id) const {
    std::shared_lock lock(mutex_);
    return entries_.find(id) != entries_.end();
}

ResourceState ResourceStorage::get_state(ResourceID id) const {
    std::shared_lock lock(mutex_);

    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return ResourceState::Unloaded;
    }

    return it->second.state;
}

void ResourceStorage::set_state(ResourceID id, ResourceState state) {
    std::unique_lock lock(mutex_);

    auto it = entries_.find(id);
    if (it != entries_.end()) {
        it->second.state = state;
    }
}

size_t ResourceStorage::get_size(ResourceID id) const {
    std::shared_lock lock(mutex_);

    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return 0;
    }

    return it->second.size;
}

void ResourceStorage::remove(ResourceID id) {
    std::unique_lock lock(mutex_);
    evict_resource_internal(id);
}

void ResourceStorage::clear() {
    std::unique_lock lock(mutex_);

    entries_.clear();
    lru_list_.clear();
    current_memory_usage_ = 0;
}

void ResourceStorage::set_budget(size_t bytes) {
    std::unique_lock lock(mutex_);
    budget_bytes_ = bytes;

    // Evict if now over budget
    while (current_memory_usage_ > budget_bytes_ && !lru_list_.empty()) {
        evict_lru();
    }
}

void ResourceStorage::evict_lru() {
    std::unique_lock lock(mutex_);

    if (lru_list_.empty()) {
        return;
    }

    // Evict least recently used (back of list)
    ResourceID id = lru_list_.back();
    evict_resource_internal(id);
}

void ResourceStorage::evict_until_budget() {
    std::unique_lock lock(mutex_);

    while (current_memory_usage_ > budget_bytes_ && !lru_list_.empty()) {
        ResourceID id = lru_list_.back();
        evict_resource_internal(id);
    }
}

void ResourceStorage::touch(ResourceID id) {
    std::unique_lock lock(mutex_);

    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }

    // Move to front of LRU list
    lru_list_.erase(it->second.lru_iter);
    lru_list_.push_front(id);
    it->second.lru_iter = lru_list_.begin();
}

void ResourceStorage::evict_resource_internal(ResourceID id) {
    // Caller must hold unique lock

    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return;
    }

    // Update memory usage
    current_memory_usage_ -= it->second.size;

    // Remove from LRU list
    lru_list_.erase(it->second.lru_iter);

    // Remove entry
    entries_.erase(it);
}

}// namespace rbc
