#include <rbc_world/resource_importer.h>
#include <rbc_world/importers/mesh_importer_obj.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_world/importers/texture_importer_stb.h>

namespace rbc::world {

/**
 * @brief Register all built-in resource importers
 * This function should be called during system initialization
 */
RBC_RUNTIME_API void register_builtin_importers() {
    auto &registry = ResourceImporterRegistry::instance();

    // Register mesh importers
    static ObjMeshImporter obj_mesh_importer;
    registry.register_importer(&obj_mesh_importer);
    
    static GltfMeshImporter gltf_mesh_importer;
    registry.register_importer(&gltf_mesh_importer);
    
    static GlbMeshImporter glb_mesh_importer;
    registry.register_importer(&glb_mesh_importer);

    // Register texture importers
    // StbTextureImporter handles multiple formats (.png, .jpg, .jpeg, .bmp, .gif, .psd, .pnm, .tga)
    // We need to register it for each extension since vstd::HashMap doesn't support iteration
    // We'll create a wrapper that delegates to a single instance
    static StbTextureImporter stb_texture_importer_png;
    registry.register_importer(&stb_texture_importer_png);

    // For other STB formats, we'll need separate instances or a different approach
    // Since StbTextureImporter::can_import checks multiple extensions, we register it once
    // and it will handle all supported formats through can_import

    static HdrTextureImporter hdr_texture_importer;
    registry.register_importer(&hdr_texture_importer);

    static TiffTextureImporter tiff_texture_importer;
    registry.register_importer(&tiff_texture_importer);

    static ExrTextureImporter exr_texture_importer;
    registry.register_importer(&exr_texture_importer);

    // Register additional STB formats with StbTextureImporter
    // We need to register StbTextureImporter for each supported extension
    // For simplicity, we'll create multiple instances or modify the registry
    // to support multiple extensions per importer
    // Alternative: Register StbTextureImporter for each extension
    // Since StbTextureImporter::supports_extension checks multiple formats,
    // we register it for the primary extension (.png) and let it handle others
    // through the can_import check
}

}// namespace rbc::world
