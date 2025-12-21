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
#include <rbc_world_v2/base_object.h>

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

struct RBC_WORLD_API IResource : world::BaseObject {
    virtual ~IResource();
    virtual void OnInstall() {}
    virtual void OnUninstall() {}
};

struct RBC_WORLD_API ResourceHandle {
public:
    ResourceHandle();
    // ResourceHandle(const ResourceHandle &other);
    // ResourceHandle &operator=(const ResourceHandle &other);
    // ResourceHandle(const skr::GUID &other);
    // ResourceHandle &operator=(const skr::GUID &other);
    // ResourceHandle(ResourceHandle &&other);
    // ResourceHandle &operator=(ResourceHandle &&other);
    // ~ResourceHandle();
    // explicit operator bool() const;
    // bool operator==(const ResourceHandle &) const;

    // void *load();
    // void *install();
    // void reset();

    // void *get_loaded() const;
    // void *get_installed() const;

    // bool is_null() const;
    // bool is_guid() const;
    // bool is_loaded() const;
    // bool is_installed() const;

    // rbc::Guid get_guid() const;
    // rbc::TypeInfo get_type() const;

    // EResourceLoadingStatus get_status() const;
    // ResourceHandle clone();

protected:
    // [[nodiscard]] void *load(bool requireInstalled);
    // ResourceRecord *get_record() const;
    // void acquire_record(ResourceRecord *record) const;

    // union {
    //     rbc::Guid guid;
    //     struct {
    //         mutable uint64_t padding;       // zero, flag for resolve
    //         mutable ResourceRecord *pointer;// fat pointer (resource record poitner & request type)
    //     };
    // };
};

struct RBC_WORLD_API ResourceHeader {
    uint32_t version = ~0ull;
    vstd::Guid guid;
    vstd::Guid type;
    luisa::vector<ResourceHandle> dependencies;
};

struct RBC_WORLD_API ResourceRequest {
    virtual ~ResourceRequest() = default;
public:
    [[nodiscard]] virtual vstd::Guid GetGuid() const = 0;
    [[nodiscard]] virtual luisa::span<const uint8_t> GetData() const = 0;
    [[nodiscard]] virtual luisa::span<const ResourceHandle> GetDependencies() const = 0;

    virtual void Update() = 0;
    virtual void Okey() = 0;
    virtual void Failed() = 0;
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
    virtual bool CancelRequestFile(ResourceRequest *request) = 0;

    void FillRequest(ResourceRequest *request, ResourceHeader header, luisa::string_view base_path, luisa::string_view uri);
};

struct RBC_WORLD_API LocalResourceRegistry {
public:
    explicit LocalResourceRegistry(luisa::string_view base_path);
    virtual ~LocalResourceRegistry() = default;
    bool RequestResourceFile(ResourceRequest *request);
    bool CancelRequestFile(ResourceRequest *request);
private:
    luisa::string_view _base_path;
};

struct RBC_WORLD_API ResourceFactory {
    virtual float AsyncSerdeLoadFactor() { return 1.0f; }
};

struct RBC_WORLD_API ResourceSystem {
    friend struct rbc::ResourceHandle;

public:
    virtual ~ResourceSystem();
};

// serde for header

}// namespace rbc