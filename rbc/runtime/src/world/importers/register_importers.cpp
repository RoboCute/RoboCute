#include <rbc_world/resource_importer.h>
#include <rbc_world/importers/mesh_importer_obj.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_world/importers/mesh_importer_ply.h>
#include <rbc_world/importers/mesh_importer_fbx.h>
#include <rbc_world/importers/mesh_importer_stl.h>
#include <rbc_world/importers/mesh_importer_3ds.h>
#include <rbc_world/importers/mesh_importer_off.h>
#include <rbc_world/importers/mesh_importer_collada.h>
#include <rbc_world/importers/mesh_importer_abc.h>
#include <rbc_world/importers/texture_importer_stb.h>
#include <rbc_world/importers/texture_importer_exr.h>
#include <rbc_world/importers/texture_importer_hdr.h>
#include <rbc_world/importers/scene_importer.h>
#include <rbc_world/importers/mat_importer.h>

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

    static FbxMeshImporter fbx_mesh_importer;
    registry.register_importer(&fbx_mesh_importer);

    static PlyMeshImporter ply_mesh_importer;
    registry.register_importer(&ply_mesh_importer);

    static StlMeshImporter stl_mesh_importer;
    registry.register_importer(&stl_mesh_importer);

    static ThreeDSMeshImporter threeds_mesh_importer;
    registry.register_importer(&threeds_mesh_importer);

    static OffMeshImporter off_mesh_importer;
    registry.register_importer(&off_mesh_importer);

    static ColladaMeshImporter collada_mesh_importer;
    registry.register_importer(&collada_mesh_importer);

    static AbcMeshImporter abc_mesh_importer;
    registry.register_importer(&abc_mesh_importer);
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

    // static TiffTextureImporter tiff_texture_importer;
    // registry.register_importer(&tiff_texture_importer);

    static ExrTextureImporter exr_texture_importer;
    registry.register_importer(&exr_texture_importer);

    static MatJsonImporter mat_json_importer;
    registry.register_importer(&mat_json_importer);

    static SceneImporter scene_importer;
    registry.register_importer(&scene_importer);

    // Anim Resources
}

}// namespace rbc::world
