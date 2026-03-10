#pragma once
#include <rbc_config.h>
#include <rbc_world/resource_importer.h>
#include <luisa/vstl/functional.h>

namespace rbc::world {

/**
 * @brief Scoped importer registry manager
 * 
 * This class provides a RAII-style wrapper for registering/unregistering
 * resource importers. It allows for modular importer registration with
 * automatic cleanup on destruction.
 * 
 * Example usage:
 * @code
 * {
 *     ImporterRegistry registry;
 *     registry.register_mesh_importers()
 *             .register_texture_importers();
 *     // Importers registered here...
 * } // Automatically unregisters all importers on scope exit
 * @endcode
 */
class ImporterRegistry {
public:
    /**
     * @brief Constructor - creates an empty registry instance
     */
    ImporterRegistry() = default;

    /**
     * @brief Destructor - automatically unregisters all importers
     */
    ~ImporterRegistry();

    // Non-copyable
    ImporterRegistry(const ImporterRegistry&) = delete;
    ImporterRegistry& operator=(const ImporterRegistry&) = delete;

    // Movable
    ImporterRegistry(ImporterRegistry&& other) noexcept;
    ImporterRegistry& operator=(ImporterRegistry&& other) noexcept;

    /**
     * @brief Register a single importer instance
     * @param importer Pointer to the importer to register
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_importer(IResourceImporter* importer);

    /**
     * @brief Unregister a previously registered importer
     * @param extension The file extension associated with the importer
     * @param type The resource type MD5
     */
    void unregister_importer(luisa::string_view extension, MD5 type);

    /**
     * @brief Register all built-in mesh importers
     * Includes: obj, gltf, glb, fbx, ply, stl, 3ds, off, collada
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_mesh_importers();

    /**
     * @brief Register all built-in texture importers
     * Includes: png, jpg, jpeg, bmp, gif, psd, pnm, tga, hdr, exr
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_texture_importers();

    /**
     * @brief Register scene importer
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_scene_importer();

    /**
     * @brief Register material importer
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_material_importer();

    /**
     * @brief Register all built-in importers at once
     * Convenience method that registers mesh, texture, scene and material importers
     * @return Reference to this registry for chaining
     */
    ImporterRegistry& register_all_builtin_importers();

    /**
     * @brief Unregister all importers registered by this instance
     */
    void unregister_all();

    /**
     * @brief Check if any importers are registered by this instance
     */
    [[nodiscard]] bool empty() const noexcept { return _registered_importers.empty(); }

    /**
     * @brief Get the number of registered importers
     */
    [[nodiscard]] size_t size() const noexcept { return _registered_importers.size(); }

private:
    struct RegisteredImporter {
        luisa::string extension;
        MD5 type;
        IResourceImporter* importer;
    };

    luisa::vector<RegisteredImporter> _registered_importers;
};

}// namespace rbc::world
