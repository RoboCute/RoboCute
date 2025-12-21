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

namespace rbc {

enum class EResourceLoadingStatus : uint32_t;
enum class EResourceInstallStatus {
    InProgress,
    Succeed,
    Failed
};

struct RBC_WORLD_API LocalResourceRegistry {
};

struct RBC_WORLD_API ResourceRecord {
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

struct RBC_WORLD_API ResourceFactory {
    virtual float AsyncSerdeLoadFactor() { return 1.0f; }
};

}// namespace rbc