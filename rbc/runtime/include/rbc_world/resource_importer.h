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

    /**
     * @brief Get the file extension this importer handles (e.g., ".obj", ".png")
     */
    [[nodiscard]] virtual luisa::string_view extension() const = 0;

    /**
     * @brief Get the resource type this importer handles
     */
    [[nodiscard]] virtual ResourceType resource_type() const = 0;

    /**
     * @brief Check if this importer can handle the given path
     */
    virtual bool can_import(luisa::filesystem::path const &path) const;
};

/**
 * @brief Importer interface for MeshResource
 */
struct RBC_RUNTIME_API IMeshImporter : IResourceImporter {
    [[nodiscard]] ResourceType resource_type() const override { return ResourceType::Mesh; }

    /**
     * @brief Import mesh data from file
     * @param resource The MeshResource to populate
     * @param path Path to the file
     * @return true if import succeeded, false otherwise
     */
    virtual bool import(MeshResource *resource, luisa::filesystem::path const &path) = 0;

protected:
    // Accessor methods for MeshResource private members
    // These methods allow derived importers to access private members
    // IMeshImporter is a friend of MeshResource, so it can access private members
    // Implementation is in resource_importer.cpp to avoid circular dependencies

    static RC<rbc::DeviceMesh> &device_mesh_ref(MeshResource *resource);
    static uint32_t &vertex_count_ref(MeshResource *resource);
    static uint32_t &triangle_count_ref(MeshResource *resource);
    static uint32_t &uv_count_ref(MeshResource *resource);
    // Bit-fields cannot return references, so use getter/setter methods
    // Implementation is in resource_importer.cpp
    static bool contained_normal(MeshResource *resource);
    static void set_contained_normal(MeshResource *resource, bool value);
    static bool contained_tangent(MeshResource *resource);
    static void set_contained_tangent(MeshResource *resource, bool value);
    static vstd::vector<uint> &submesh_offsets_ref(MeshResource *resource);
};

/**
 * @brief Importer interface for TextureResource
 */
struct RBC_RUNTIME_API ITextureImporter : IResourceImporter {
    [[nodiscard]] ResourceType resource_type() const override { return ResourceType::Texture; }

    /**
     * @brief Import texture data from file
     * @param loader The TextureLoader instance (for processing)
     * @param path Path to the file
     * @param mip_level Desired mip level
     * @param to_vt Whether to convert to virtual texture
     * @return RC<TextureResource> if import succeeded, nullptr otherwise
     */
    virtual RC<TextureResource> import(
        TextureLoader *loader,
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt) = 0;

protected:
    // Accessor methods for TextureResource private members
    // These methods allow derived importers to access private members
    // ITextureImporter is a friend of TextureResource, so it can access private members
    // Implementation is in resource_importer.cpp to avoid circular dependencies

    static RC<rbc::DeviceResource> &tex_ref(TextureResource *resource);
    static luisa::uint2 &size_ref(TextureResource *resource);
    static LCPixelStorage &pixel_storage_ref(TextureResource *resource);
    static uint32_t &mip_level_ref(TextureResource *resource);
    static bool &is_vt_ref(TextureResource *resource);
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
    mutable luisa::spin_mutex _mtx;

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
