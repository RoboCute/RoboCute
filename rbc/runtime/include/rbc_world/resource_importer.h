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
}// namespace rbc

namespace rbc::world {

// Forward declarations
struct MeshResource;
struct TextureResource;
struct TextureLoader;

/**
 * @brief Resource type identifier
 */
enum class ResourceType : uint32_t {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Material = 3,
    Shader = 4,
    AnimSequence = 5,
    Skeleton = 6,
    Skin = 9,
    AnimGraph = 10,
    SkelMesh = 11,
    Custom = 1000
};

/**
 * @brief Base interface for resource importers
 * Each importer handles a specific (extension, resource_type) combination
 */
struct RBC_RUNTIME_API IResourceImporter {
    virtual ~IResourceImporter() = default;
    [[nodiscard]] virtual luisa::string_view extension() const = 0;
    [[nodiscard]] virtual ResourceType resource_type() const = 0;
    [[nodiscard]] virtual bool can_import(luisa::filesystem::path const &path) const;
};

/**
 * @brief Importer interface for MeshResource
 */
struct RBC_RUNTIME_API IMeshImporter : IResourceImporter {
    [[nodiscard]] ResourceType resource_type() const override { return ResourceType::Mesh; }
    [[nodiscard]] virtual bool import(MeshResource *resource, luisa::filesystem::path const &path) = 0;
};

struct RBC_RUNTIME_API ITextureImporter : IResourceImporter {
    [[nodiscard]] ResourceType resource_type() const override { return ResourceType::Texture; }

    [[nodiscard]] virtual bool import(
        RC<TextureResource> resource,
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) = 0;
};

/**
 * @brief Registry for resource importers
 * Uses (extension, resource_type) pair as key
 */
struct RBC_RUNTIME_API ResourceImporterRegistry {
    using ExtensionKey = luisa::string;
    using ImporterKey = std::pair<ExtensionKey, ResourceType>;

    struct KeyHash {
        size_t operator()(ImporterKey const &key) const {
            return luisa::hash<ExtensionKey>{}(key.first) ^
                   (static_cast<size_t>(key.second) << 16);
        }
    };

    struct KeyEqual {
        int32_t operator()(ImporterKey const &lhs, ImporterKey const &rhs) const {
            if (lhs.first < rhs.first) return -1;
            if (lhs.first > rhs.first) return 1;
            if (lhs.second < rhs.second) return -1;
            if (lhs.second > rhs.second) return 1;
            return 0;// equal
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
    void unregister_importer(luisa::string_view extension, ResourceType type);

    /**
     * @brief Find an importer for the given extension and resource type
     */
    IResourceImporter *find_importer(luisa::string_view extension, ResourceType type) const;

    /**
     * @brief Find an importer for the given path and resource type
     */
    IResourceImporter *find_importer(luisa::filesystem::path const &path, ResourceType type) const;

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
