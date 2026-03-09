#include "test_util.h"

#include <rbc_core/type_info.h>
#include <rbc_core/rc.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world/importers/mesh_importer_obj.h>
#include <rbc_world/importers/mesh_importer_fbx.h>
#include <rbc_world/importers/mesh_importer_gltf.h>
#include <rbc_world/importers/mesh_importer_stl.h>
#include <rbc_world/importers/mesh_importer_ply.h>
#include <rbc_world/importers/mesh_importer_off.h>
#include <rbc_world/importers/mesh_importer_3ds.h>
#include <rbc_world/importers/mesh_importer_collada.h>
#include <rbc_world/importers/mesh_importer_abc.h>
#include <rbc_world/importers/register_importers.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/base_object.h>
#include <luisa/core/logging.h>

using namespace rbc;
using namespace rbc::world;

struct WorldFixture {
    luisa::vector<RC<BaseObject>> _objects;
    WorldFixture() {
        rbc::RuntimeStaticBase::init_all();
        init_world({}, {});
        // Register all mesh importers
        register_builtin_importers();
    }
    template<typename T>
    T *create() {
        auto obj = rbc::world::create_object<T>();
        _objects.emplace_back(obj);
        return obj;
    }
    ~WorldFixture() {
        // Clear all objects before destroying world
        _objects.clear();
        destroy_world();
        rbc::RuntimeStaticBase::dispose_all();
    }
};

TEST_SUITE("model") {

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_obj") {
        ObjMeshImporter importer;
        CHECK(importer.extension() == ".obj");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        // Test importing non-existent file returns false
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.obj");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_fbx") {
        FbxMeshImporter importer;
        CHECK(importer.extension() == ".fbx");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.fbx");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_gltf") {
        GltfMeshImporter importer;
        CHECK(importer.extension() == ".gltf");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.gltf");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_glb") {
        GlbMeshImporter importer;
        CHECK(importer.extension() == ".glb");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.glb");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_stl") {
        StlMeshImporter importer;
        CHECK(importer.extension() == ".stl");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.stl");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_ply") {
        PlyMeshImporter importer;
        CHECK(importer.extension() == ".ply");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.ply");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_off") {
        OffMeshImporter importer;
        CHECK(importer.extension() == ".off");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.off");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_3ds") {
        ThreeDSMeshImporter importer;
        CHECK(importer.extension() == ".3ds");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.3ds");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_collada") {
        ColladaMeshImporter importer;
        CHECK(importer.extension() == ".dae");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.dae");
        CHECK_FALSE(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_abc") {
        AbcMeshImporter importer;
        CHECK(importer.extension() == ".abc");
        CHECK(importer.resource_type() == MD5{"rbc::world::MeshResource"sv});
        
        auto mesh = create<MeshResource>();
        auto result = importer.import(mesh, "non_existent_file.abc");
        CHECK_FALSE(result);
    }

    
    TEST_CASE_FIXTURE(WorldFixture, "importer_registry") {
        auto &registry = ResourceImporterRegistry::instance();
        
        // Test finding importers by extension
        auto mesh_type = MD5{"rbc::world::MeshResource"sv};
        
        CHECK(registry.find_importer(luisa::string_view{".obj"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".fbx"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".gltf"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".glb"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".stl"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".ply"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".off"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".3ds"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".dae"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".abc"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".lwo"}, mesh_type) != nullptr);
        
        // Test case insensitivity for extensions
        CHECK(registry.find_importer(luisa::string_view{".OBJ"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".Fbx"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".GLTF"}, mesh_type) != nullptr);
        
        // Test non-existent extension returns nullptr
        CHECK(registry.find_importer(luisa::string_view{".nonexistent"}, mesh_type) == nullptr);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_resource_creation") {
        auto mesh = create<MeshResource>();
        CHECK(mesh != nullptr);
        CHECK(mesh->empty());
        CHECK(mesh->vertex_count() == 0);
        CHECK(mesh->triangle_count() == 0);
        CHECK(mesh->uv_count() == 0);
        CHECK_FALSE(mesh->contained_normal());
        CHECK_FALSE(mesh->contained_tangent());
    }
}
