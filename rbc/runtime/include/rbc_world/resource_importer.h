#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/stl/string.h>
#include <luisa/vstl/common.h>
#include <rbc_world/resource_base.h>
#include <rbc_plugin/generated/resource_meta.hpp>

namespace rbc {
struct DeviceMesh;
struct DeviceResource;
struct TextureLoader;
}// namespace rbc

namespace rbc::world {

// Forward declarations
struct MaterialResource;
struct MeshResource;
struct TextureResource;

/**
 * @brief Base interface for resource importers
 * Each importer handles a specific (extension, resource_type) combination
 */
struct IResourceImporter {
    virtual ~IResourceImporter() = default;
    [[nodiscard]] virtual luisa::string_view extension() const = 0;
    [[nodiscard]] virtual MD5 resource_type() const = 0;
    [[nodiscard]] virtual bool can_import(luisa::filesystem::path const &path) const;
    [[nodiscard]] virtual bool import(Resource *res, luisa::filesystem::path const &path) = 0;
    [[nodiscard]] RBC_RUNTIME_API RC<Resource> import(
        vstd::Guid guid,
        luisa::filesystem::path const &path,
        luisa::string const &meta_json);
};

/**
 * @brief Importer interface for MeshResource
 */
struct IMeshImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return MD5{"rbc::world::MeshResource"sv};
    }
};

struct ITextureImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return MD5{"rbc::world::TextureResource"sv};
    }

    [[nodiscard]] virtual bool import(
        RC<TextureResource> resource,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) = 0;
    RBC_RUNTIME_API bool import(Resource *resource, luisa::filesystem::path const &path) override;
};

/**
 * @brief Registry for resource importers
 * Uses (extension, resource_type) pair as key
 */
struct RBC_RUNTIME_API ResourceImporterRegistry {
    using ExtensionKey = luisa::string;
    using ImporterKey = std::pair<ExtensionKey, MD5>;

    struct KeyHash {
        size_t operator()(ImporterKey const &key) const {
            return luisa::hash<ExtensionKey>{}(key.first, luisa::hash<MD5>{}(key.second));
        }
    };

    struct KeyEqual {
        int32_t operator()(ImporterKey const &lhs, ImporterKey const &rhs) const {
            if (lhs.first < rhs.first) return -1;
            if (lhs.first > rhs.first) return 1;
            return std::memcmp(&lhs.second, &rhs.second, sizeof(MD5));
        }
    };

private:
    vstd::HashMap<ImporterKey, IResourceImporter *, KeyHash, KeyEqual> _importers;
    mutable rbc::shared_atomic_mutex _mtx;

public:
    /**
     * @brief Register an importer
     */
    void register_importer(IResourceImporter *importer);

    /**
     * @brief Unregister an importer
     */
    void unregister_importer(luisa::string_view extension, MD5 type);

    /**
     * @brief Find an importer for the given extension and resource type
     */
    IResourceImporter *find_importer(luisa::string_view extension, MD5 type) const;

    /**
     * @brief Find an importer for the given path and resource type
     */
    IResourceImporter *find_importer(luisa::filesystem::path const &path, MD5 type) const;

    /**
     * @brief Get the global registry instance
     */
    static ResourceImporterRegistry &instance();
};

/**
 * @brief Helper function to normalize extension (lowercase, with dot)
 */
RBC_RUNTIME_API luisa::string normalize_extension(luisa::string_view ext);

}// namespace rbc::world
