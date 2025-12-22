#pragma once
/**
 * Core Resource Declaration
 * ResourceSystem
 * LocalResourceRegistry
 * ResourceRequest
 * ResourceHandle
 * ResourceHeader
 */
#include <rbc_config.h>
#include "rbc_core/blob.h"
#include <rbc_core/hash.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <rbc_world_v2/base_object.h>
#include <luisa/core/fiber.h>

namespace rbc {
struct ResourceSystem;
struct ResourceFactory;
struct ResourceRecord;

}// namespace rbc

namespace rbc {

enum class EResourceLoadingStatus : uint32_t;
enum class EResourceInstallStatus {
    InProgress,
    Succeed,
    Failed
};
enum class EResourceLoadingStatus : uint32_t {
    Unloaded,
    Loading,
    Loaded,
    WaitingDependencies,
    Installing,
    Installed,
    Uninstalling,
    Unloading,
    Error,
    Count
};

struct RBC_WORLD_API IResource {
    virtual ~IResource();
    virtual void OnInstall() {}
    virtual void OnUninstall() {}
};

struct RBC_WORLD_API ResourceHandle {
public:
    ResourceHandle();
    ResourceHandle(const ResourceHandle &other);
    ResourceHandle &operator=(const ResourceHandle &other);
    ResourceHandle(const vstd::Guid &other);
    ResourceHandle &operator=(const vstd::Guid &other);
    ResourceHandle(ResourceHandle &&other);
    ResourceHandle &operator=(ResourceHandle &&other);
    ~ResourceHandle();
    explicit operator bool() const;
    bool operator==(const ResourceHandle &) const;

    void *load();
    void *install();
    void reset();

    void *get_loaded() const;
    void *get_installed() const;

    bool is_null() const;
    bool is_guid() const;
    bool is_loaded() const;
    bool is_installed() const;

    vstd::Guid get_guid() const;
    rbc::TypeInfo get_type() const;

    EResourceLoadingStatus get_status() const;
    ResourceHandle clone();

protected:
    friend struct ResourceSystem;

    [[nodiscard]] void *load(bool requireInstalled);
    ResourceRecord *get_record() const;
    void acquire_record(ResourceRecord *record) const;

    union {
        vstd::Guid guid;
        struct {
            mutable uint64_t padding;       // zero, flag for resolve
            mutable ResourceRecord *pointer;// fat pointer (resource record poitner & request type)
        };
    };
};

struct RBC_WORLD_API ResourceHeader {
    uint32_t version{~0u};
    vstd::Guid guid;
    rbc::TypeInfo type{rbc::TypeInfo::invalid()};
    luisa::vector<ResourceHandle> dependencies;

    void serialize(rbc::JsonSerializer &obj) const;
    void deserialize(rbc::JsonDeSerializer &obj);
};

struct RBC_WORLD_API ResourceRequest {
    virtual ~ResourceRequest() = default;
public:
    [[nodiscard]] vstd::Guid GetGuid() const;
    [[nodiscard]] luisa::span<const uint8_t> GetData() const;
    [[nodiscard]] luisa::span<const ResourceHandle> GetDependencies() const;

    void Update();
    bool Okay();
    bool Failed();

protected:
    friend class ResourceSystem;
    friend class ResourceRegistry;

    void UnloadDependenciesInternal();
    std::atomic_bool requireInstall = false;
    ResourceSystem *system;
    ResourceFactory *factory;
    ResourceRecord *resource_record;

    // vfs
    // io_future
    luisa::string resource_url;// resource url
    rbc::BlobId blob;          // the actual binary data

    luisa::fiber::event serde_event;
    std::mutex update_mtx;
};

struct RBC_WORLD_API ResourceRecord {
    ResourceHeader header;
    std::atomic<uint32_t> reference_count = 0;
    IResource *resource = nullptr;
    std::atomic<EResourceLoadingStatus> loading_status = EResourceLoadingStatus::Unloaded;
    ResourceRequest *active_request;

    void SetStatus(EResourceLoadingStatus InStatus);
    uint32_t AddReference();
    void RemoveReference();
    bool isReferenced() const;
};

struct RBC_WORLD_API ResourceRegistry {
public:
    virtual bool RequestResourceFile(ResourceRequest *request) = 0;

    void FillRequest(ResourceRequest *request, ResourceHeader header, luisa::string_view base_path, luisa::string_view uri);
};

struct RBC_WORLD_API LocalResourceRegistry {
public:
    explicit LocalResourceRegistry(luisa::string_view base_path);
    virtual ~LocalResourceRegistry() = default;
    bool RequestResourceFile(ResourceRequest *request);
private:
    luisa::string_view _base_path;
};

struct RBC_WORLD_API ResourceFactory {
    virtual rbc::TypeInfo GetResourceType() = 0;
    virtual bool AsyncIO() { return true; }
    virtual float AsyncSerdeLoadFactor() { return 1.0f; }
    virtual EResourceInstallStatus Install(ResourceRecord *record) { return EResourceInstallStatus::Succeed; }
    virtual bool Uninstall(ResourceRecord *record) { return true; }
    virtual EResourceInstallStatus UpdateInstall(ResourceRecord *record) {
        return EResourceInstallStatus::Succeed;
    }
};

struct RBC_WORLD_API ResourceSystem {
    friend struct rbc::ResourceHandle;

public:
    ResourceSystem();
    virtual ~ResourceSystem();

    void Initialize(ResourceRegistry *provider);
    bool IsInitialized();
    void Shutdown();
    void Update();

    ResourceRecord *RegisterResource(const vstd::Guid &guid);
    ResourceRecord *LoadResource(const ResourceHandle handle, bool requireInstall);
    ResourceRecord *FindResourceRecord(const vstd::Guid &guid);
    ResourceHandle EnqueueResource(vstd::Guid guid, rbc::TypeInfo type, rbc::IResource *resource, bool requireInstall, luisa::vector<ResourceHandle> dependencies, EResourceLoadingStatus status);

    void UnloadResource(const ResourceHandle handle);
    void FlushResource(const ResourceHandle handle);

    [[nodiscard]] ResourceFactory *FindFactory(rbc::TypeInfo type) const;
    void RegisterFactory(ResourceFactory *factory);
    void UnregisterFactory(rbc::TypeInfo type);

    [[nodiscard]] ResourceRegistry *GetRegistry() const;

protected:
    void UnloadResourceInternal(ResourceRecord *record);
    void DestroyRecordInternal(ResourceRecord *record);
    void ClearFinishedRequestsInternal();

    ResourceRegistry *resource_registry = nullptr;

    // concurrent queue
    rbc::ConcurrentQueue<ResourceRequest *> request_queue;

    std::mutex requests_mtx;
    luisa::vector<ResourceRequest *> failed_requests;
    luisa::vector<ResourceRequest *> to_update_requests;
    luisa::vector<ResourceRequest *> serde_batch;

    bool quit = false;

    luisa::spin_mutex resource_records_mtx;
    vstd::HashMap<vstd::Guid, ResourceRecord *, luisa::hash<vstd::Guid>> resource_records;

    luisa::spin_mutex resource_to_update_mtx;
    vstd::HashMap<vstd::Guid, ResourceRecord *, luisa::hash<vstd::Guid>> resource_to_record;

    luisa::spin_mutex resource_factories_mtx;
    vstd::HashMap<rbc::TypeInfo, ResourceFactory *, luisa::hash<rbc::TypeInfo>> resource_factories;
};

// global singleton get
RBC_WORLD_API ResourceSystem *GetResourceSystem();

template<class T>
struct AsyncResource : public ResourceHandle {
public:
    AsyncResource() RBC_NOEXCEPT;
    AsyncResource(const vstd::Guid &guid) RBC_NOEXCEPT;
    AsyncResource(const ResourceHandle &hdl) RBC_NOEXCEPT;
    AsyncResource(const AsyncResource &hdl) RBC_NOEXCEPT;

    inline T *load() { return (T *)ResourceHandle::load(); }
    inline T *install() { return (T *)ResourceHandle::install(); }
    inline T *get_loaded() { return (T *)ResourceHandle::get_loaded(); }
    inline T *get_installed() { return (T *)ResourceHandle::get_installed(); }

    inline AsyncResource clone() { return *this; }
};

template<class T>
inline AsyncResource<T>::AsyncResource() RBC_NOEXCEPT
    : ResourceHandle() {
}

template<class T>
inline AsyncResource<T>::AsyncResource(const vstd::Guid &guid) RBC_NOEXCEPT
    : ResourceHandle(guid) {
}

template<class T>
inline AsyncResource<T>::AsyncResource(const ResourceHandle &hdl) RBC_NOEXCEPT
    : ResourceHandle(hdl) {
}

template<class T>
inline AsyncResource<T>::AsyncResource(const AsyncResource &hdl) RBC_NOEXCEPT
    : ResourceHandle(hdl) {
}

}// namespace rbc