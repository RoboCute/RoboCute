#include "rbc_world/async_resource_loader.h"
#include "rbc_world/resource_loader.h"
#include "rbc_world/resource_storage.h"
#include <iostream>

namespace rbc {

AsyncResourceLoader::AsyncResourceLoader()
    : storage_(luisa::make_unique<ResourceStorage>(cache_budget_)) {
}

AsyncResourceLoader::~AsyncResourceLoader() {
    shutdown();
}

void AsyncResourceLoader::initialize(size_t num_threads) {
    if (running_) {
        return;
    }

    running_ = true;

    // Start worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&AsyncResourceLoader::worker_thread, this);
    }

    std::cout << "[AsyncResourceLoader] Initialized with " << num_threads << " threads\n";
}

void AsyncResourceLoader::shutdown() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Wake up all worker threads
    queue_cv_.notify_all();

    // Join all threads
    for (auto &thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    worker_threads_.clear();
    storage_->clear();

    std::cout << "[AsyncResourceLoader] Shutdown complete\n";
}

void AsyncResourceLoader::set_cache_budget(size_t bytes) {
    cache_budget_ = bytes;
    if (storage_) {
        storage_->set_budget(bytes);
    }
}

size_t AsyncResourceLoader::get_memory_usage() const {
    return storage_ ? storage_->get_memory_usage() : 0;
}

bool AsyncResourceLoader::load_resource(ResourceID id, uint32_t type_value,
                                        const std::string &path,
                                        const std::string &options_json) {
    if (!running_) {
        initialize();// Auto-initialize if not started
    }

    // Check if already loaded
    if (is_loaded(id)) {
        return true;
    }

    // Create request
    ResourceRequest request;
    request.id = id;
    request.type = static_cast<ResourceType>(type_value);
    request.path = path;
    request.priority = LoadPriority::Normal;
    request.timestamp = std::chrono::steady_clock::now();

    // Add to queue
    {
        std::lock_guard lock(queue_mutex_);
        request_queue_.push(request);
    }
    queue_cv_.notify_one();

    return true;
}

bool AsyncResourceLoader::is_loaded(ResourceID id) const {
    return storage_ && storage_->get_state(id) == ResourceState::Loaded;
}

uint8_t AsyncResourceLoader::get_state(ResourceID id) const {
    if (!storage_) {
        return static_cast<uint8_t>(ResourceState::Unloaded);
    }
    return static_cast<uint8_t>(storage_->get_state(id));
}

size_t AsyncResourceLoader::get_resource_size(ResourceID id) const {
    return storage_ ? storage_->get_size(id) : 0;
}

void *AsyncResourceLoader::get_resource_data(ResourceID id) {
    if (!storage_) {
        return nullptr;
    }

    auto data = storage_->get(id);
    return data ? data.get() : nullptr;
}

void AsyncResourceLoader::unload_resource(ResourceID id) {
    if (storage_) {
        storage_->remove(id);
    }
}

void AsyncResourceLoader::clear_unused_resources() {
    if (storage_) {
        storage_->evict_until_budget();
    }
}

void AsyncResourceLoader::register_loader(ResourceType type, LoaderFactory factory) {
    std::lock_guard lock(mutex_);
    loader_factories_[type] = std::move(factory);
}

void AsyncResourceLoader::worker_thread() {
    while (running_) {
        ResourceRequest request;

        // Wait for request
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !running_ || !request_queue_.empty();
            });

            if (!running_) {
                break;
            }

            if (request_queue_.empty()) {
                continue;
            }

            request = request_queue_.front();
            request_queue_.pop();
        }

        // Process request
        try {
            // Set state to loading
            storage_->set_state(request.id, ResourceState::Loading);

            // Load resource
            bool success = load_resource_impl(request.id, request.type,
                                              request.path, "{}");

            // Update state
            if (success) {
                storage_->set_state(request.id, ResourceState::Loaded);
            } else {
                storage_->set_state(request.id, ResourceState::Failed);
                std::cerr << "[AsyncResourceLoader] Failed to load resource: "
                          << request.path << "\n";
            }

            // Call completion callback if exists
            if (request.on_complete) {
                request.on_complete(request.id, success);
            }

        } catch (const std::exception &e) {
            std::cerr << "[AsyncResourceLoader] Exception loading "
                      << request.path << ": " << e.what() << "\n";
            storage_->set_state(request.id, ResourceState::Failed);

            if (request.on_complete) {
                request.on_complete(request.id, false);
            }
        }
    }
}

ResourceLoader *AsyncResourceLoader::get_loader(ResourceType type) {
    std::lock_guard lock(mutex_);

    // Check if loader already exists
    auto it = loaders_.find(type);
    if (it != loaders_.end()) {
        return it->second.get();
    }

    // Create loader from factory
    auto factory_it = loader_factories_.find(type);
    if (factory_it != loader_factories_.end()) {
        loaders_[type] = factory_it->second();
        return loaders_[type].get();
    }

    return nullptr;
}

bool AsyncResourceLoader::load_resource_impl(ResourceID id, ResourceType type,
                                             const std::string &path,
                                             const std::string &options_json) {
    // Get appropriate loader
    ResourceLoader *loader = get_loader(type);
    if (!loader) {
        std::cerr << "[AsyncResourceLoader] No loader registered for type "
                  << static_cast<uint32_t>(type) << "\n";
        return false;
    }

    // Check if loader can handle this file
    if (!loader->can_load(path)) {
        std::cerr << "[AsyncResourceLoader] Loader cannot handle file: " << path << "\n";
        return false;
    }

    // Load resource
    auto data = loader->load(path, options_json);
    if (!data) {
        return false;
    }

    // Get size
    size_t size = loader->get_resource_size(data);

    // Store in storage
    storage_->store(id, data, type, size);

    return true;
}

}// namespace rbc