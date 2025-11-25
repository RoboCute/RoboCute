#pragma once
#include "rbc_config.h"
#include "rbc_world/resource.h"
#include "rbc_world/resource_request.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

namespace rbc {

class ResourceLoader;
class ResourceStorage;

/**
 * 异步资源加载器
 * - 管理I/O线程池
 * - 解析资源文件
 * - 上传到GPU (如果需要)
 */
struct RBC_WORLD_API AsyncResourceLoader {
public:
    AsyncResourceLoader();
    ~AsyncResourceLoader();

    // === Initialization ===
    void initialize(size_t num_threads = 4);
    void shutdown();

    // === Cache Budget ===
    void set_cache_budget(size_t bytes);
    size_t get_cache_budget() const { return cache_budget_; }
    size_t get_memory_usage() const;

    // === Resource Loading ===
    bool load_resource(ResourceID id, uint32_t type_value,
                       const std::string &path,
                       const std::string &options_json);

    bool is_loaded(ResourceID id) const;
    uint8_t get_state(ResourceID id) const;
    size_t get_resource_size(ResourceID id) const;

    // === Resource Access ===
    // Generic interface (returns void*, for Python use)
    void *get_resource_data(ResourceID id);

    // === Resource Unloading ===
    void unload_resource(ResourceID id);
    void clear_unused_resources();

    // === Register Custom Loader ===
    using LoaderFactory = std::function<luisa::unique_ptr<ResourceLoader>()>;
    void register_loader(ResourceType type, LoaderFactory factory);

private:
    // Worker thread function
    void worker_thread();

    // Get loader by type
    ResourceLoader *get_loader(ResourceType type);

    // Actual loading logic
    bool load_resource_impl(ResourceID id, ResourceType type,
                            const std::string &path,
                            const std::string &options_json);

    // Thread pool
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};

    // Request queue
    std::queue<ResourceRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Resource storage
    luisa::unique_ptr<ResourceStorage> storage_;

    // Loader factories and instances
    std::unordered_map<ResourceType, LoaderFactory> loader_factories_;
    std::unordered_map<ResourceType, luisa::unique_ptr<ResourceLoader>> loaders_;

    // Cache management
    size_t cache_budget_{static_cast<size_t>(2048 * 1024 * 1024)};// 2GB default
    mutable std::mutex mutex_;
};

}// namespace rbc