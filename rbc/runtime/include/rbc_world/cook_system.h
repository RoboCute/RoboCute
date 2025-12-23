#pragma once

#include <rbc_core/blob.h>

namespace rbc {
struct CookContext;

struct RBC_RUNTIME_API AssetMetadata : RCBase {
    AssetMetadata();
    virtual ~AssetMetadata();
};

struct RBC_RUNTIME_API AssetMetaFile final {
};

struct RBC_RUNTIME_API AssetImporter {
    AssetImporter();
    virtual ~AssetImporter() = default;

    virtual void *Import(CookContext *context) = 0;
    virtual void Destroy(void *) = 0;
};

struct RBC_RUNTIME_API CookContext {
    friend class CookSystem;

public:
    virtual ~CookContext() = default;
    virtual AssetImporter *GetImporter() const = 0;

    // RegisterCooker
    // UnregisterCooker
    // GetCooker
    // EnsureCooked
    // AddCookTask
    // ImportAssetMeta
    // LoadAssetMeta
    // SaveAssetMeta
    // ParallelForEachAsset
};

}// namespace rbc