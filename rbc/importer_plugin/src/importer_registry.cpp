#include <rbc_importer/mesh_importer_obj.h>
#include <rbc_importer/mesh_importer_gltf.h>
#include <rbc_importer/mesh_importer_ply.h>
#include <rbc_importer/mesh_importer_fbx.h>
#include <rbc_importer/mesh_importer_stl.h>
#include <rbc_importer/mesh_importer_3ds.h>
#include <rbc_importer/mesh_importer_off.h>
#include <rbc_importer/mesh_importer_collada.h>
// #include <rbc_importer/mesh_importer_abc.h>
#include <rbc_importer/texture_importer_stb.h>
#include <rbc_importer/texture_importer_exr.h>
#include <rbc_importer/texture_importer_hdr.h>
#include <rbc_importer/scene_importer.h>
#include <rbc_importer/mat_importer.h>
#include <rbc_importer/importer_registry.h>

namespace rbc::world {

ImporterRegistry::~ImporterRegistry() {
    unregister_all();
}

ImporterRegistry::ImporterRegistry(ImporterRegistry&& other) noexcept
    : _registered_importers(std::move(other._registered_importers)) {}

ImporterRegistry& ImporterRegistry::operator=(ImporterRegistry&& other) noexcept {
    if (this != &other) {
        unregister_all();
        _registered_importers = std::move(other._registered_importers);
    }
    return *this;
}

ImporterRegistry& ImporterRegistry::register_importer(IResourceImporter* importer) {
    if (!importer) return *this;

    auto& global_registry = ResourceImporterRegistry::instance();
    global_registry.register_importer(importer);

    _registered_importers.emplace_back(
        luisa::string(importer->extension()),
        importer->resource_type(),
        importer);

    return *this;
}

void ImporterRegistry::unregister_importer(luisa::string_view extension, MD5 type) {
    auto& global_registry = ResourceImporterRegistry::instance();
    global_registry.unregister_importer(extension, type);

    auto it = std::remove_if(
        _registered_importers.begin(),
        _registered_importers.end(),
        [extension, type](const RegisteredImporter& reg) {
            return reg.extension == extension && reg.type == type;
        });
    _registered_importers.erase(it, _registered_importers.end());
}

void ImporterRegistry::unregister_all() {
    if (_registered_importers.empty()) return;
    auto& global_registry = ResourceImporterRegistry::instance();
    for (const auto& reg : _registered_importers) {
        global_registry.unregister_importer(reg.extension, reg.type);
    }
    _registered_importers.clear();
}

namespace {
// Static storage for importer instances
// These are kept alive for the duration of the program
struct MeshImporterInstances {
    static ObjMeshImporter& obj() {
        static ObjMeshImporter instance;
        return instance;
    }
    static GltfMeshImporter& gltf() {
        static GltfMeshImporter instance;
        return instance;
    }
    static GlbMeshImporter& glb() {
        static GlbMeshImporter instance;
        return instance;
    }
    static FbxMeshImporter& fbx() {
        static FbxMeshImporter instance;
        return instance;
    }
    static PlyMeshImporter& ply() {
        static PlyMeshImporter instance;
        return instance;
    }
    static StlMeshImporter& stl() {
        static StlMeshImporter instance;
        return instance;
    }
    static ThreeDSMeshImporter& threeds() {
        static ThreeDSMeshImporter instance;
        return instance;
    }
    static OffMeshImporter& off() {
        static OffMeshImporter instance;
        return instance;
    }
    static ColladaMeshImporter& collada() {
        static ColladaMeshImporter instance;
        return instance;
    }
};

struct TextureImporterInstances {
    static StbTextureImporter& stb() {
        static StbTextureImporter instance;
        return instance;
    }
    static HdrTextureImporter& hdr() {
        static HdrTextureImporter instance;
        return instance;
    }
    static ExrTextureImporter& exr() {
        static ExrTextureImporter instance;
        return instance;
    }
};

struct OtherImporterInstances {
    static SceneImporter& scene() {
        static SceneImporter instance;
        return instance;
    }
    static MatJsonImporter& mat() {
        static MatJsonImporter instance;
        return instance;
    }
};
}// namespace

ImporterRegistry& ImporterRegistry::register_mesh_importers() {
    register_importer(&MeshImporterInstances::obj());
    register_importer(&MeshImporterInstances::gltf());
    register_importer(&MeshImporterInstances::glb());
    register_importer(&MeshImporterInstances::fbx());
    register_importer(&MeshImporterInstances::ply());
    register_importer(&MeshImporterInstances::stl());
    register_importer(&MeshImporterInstances::threeds());
    register_importer(&MeshImporterInstances::off());
    register_importer(&MeshImporterInstances::collada());
    return *this;
}

ImporterRegistry& ImporterRegistry::register_texture_importers() {
    register_importer(&TextureImporterInstances::stb());
    register_importer(&TextureImporterInstances::hdr());
    register_importer(&TextureImporterInstances::exr());
    return *this;
}

ImporterRegistry& ImporterRegistry::register_scene_importer() {
    register_importer(&OtherImporterInstances::scene());
    return *this;
}

ImporterRegistry& ImporterRegistry::register_material_importer() {
    register_importer(&OtherImporterInstances::mat());
    return *this;
}

ImporterRegistry& ImporterRegistry::register_all_builtin_importers() {
    return register_mesh_importers()
        .register_texture_importers()
        .register_scene_importer()
        .register_material_importer();
}

}// namespace rbc::world
