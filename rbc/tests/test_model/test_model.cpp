#include "test_util.h"

// Define SKIP_IF for doctest versions that don't have it
#ifndef SKIP_IF
#define SKIP_IF(cond) if (cond) { return; }
#endif

#include <rbc_core/type_info.h>
#include <rbc_core/rc.h>
#include <rbc_core/runtime_static.h>
// #include <rbc_importer/mesh_importer_obj.h>
// #include <rbc_importer/mesh_importer_fbx.h>
// #include <rbc_importer/mesh_importer_gltf.h>
// #include <rbc_importer/mesh_importer_stl.h>
// #include <rbc_importer/mesh_importer_ply.h>
// #include <rbc_importer/mesh_importer_off.h>
// #include <rbc_importer/mesh_importer_3ds.h>
// #include <rbc_importer/mesh_importer_collada.h>
// #include <rbc_importer/mesh_importer_abc.h>
#include <rbc_world/importers/register_importers.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/base_object.h>
#include <luisa/core/logging.h>

using namespace rbc;
using namespace rbc::world;

// Helper to check if a specific model type should be tested
// Usage: --test-model=obj (or --test-model=fbx, gltf, glb, stl, ply, off, 3ds, dae)
// If not specified, all tests run
static bool should_test_model_type(luisa::string_view type) {
    for (int i = 0; i < sail::test::argc(); ++i) {
        luisa::string_view arg(sail::test::argv()[i]);
        if (arg.starts_with("--test-model=")) {
            auto model_type = arg.substr(13); // length of "--test-model="
            return model_type == type;
        }
    }
    return true; // No filter specified, test all
}

struct WorldFixture {
    luisa::fiber::scheduler scheduler;
    luisa::vector<RC<BaseObject>> _objects;
    WorldFixture() {
        rbc::RuntimeStaticBase::init_all();
        init_world("test_scene", {});
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
        SKIP_IF(!should_test_model_type("obj"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".obj"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".obj");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        // Test importing non-existent file returns false
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.obj");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_fbx") {
        SKIP_IF(!should_test_model_type("fbx"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".fbx"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".fbx");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.fbx");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_gltf") {
        SKIP_IF(!should_test_model_type("gltf"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".gltf"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".gltf");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.gltf");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_glb") {
        SKIP_IF(!should_test_model_type("glb"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".glb"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".glb");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.glb");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_stl") {
        SKIP_IF(!should_test_model_type("stl"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".stl"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".stl");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.stl");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_ply") {
        SKIP_IF(!should_test_model_type("ply"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".ply"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".ply");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.ply");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_off") {
        SKIP_IF(!should_test_model_type("off"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".off"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".off");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.off");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_3ds") {
        SKIP_IF(!should_test_model_type("3ds"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".3ds"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".3ds");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.3ds");
        CHECK(result);
    }

    TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_collada") {
        SKIP_IF(!should_test_model_type("dae"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".dae"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        CHECK(importer != nullptr);
        CHECK(importer->extension() == ".dae");
        CHECK(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
        auto mesh = create<MeshResource>();
        auto result = importer->import(mesh, "test_model.dae");
        CHECK(result);
    }

    // TEST_CASE_FIXTURE(WorldFixture, "mesh_importer_abc") {
    //     AbcMeshImporter importer;
    //     CHECK(importer.extension() == ".abc");
    //     CHECK(importer.resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5());
        
    //     auto mesh = create<MeshResource>();
    //     auto result = importer.import(mesh, "test_model.abc");
    //     CHECK_FALSE(result);
    // }

    
    TEST_CASE_FIXTURE(WorldFixture, "importer_registry") {
        auto &registry = ResourceImporterRegistry::instance();
        
        // Test finding importers by extension
        auto mesh_type = TypeInfo::get<rbc::world::MeshResource>().md5();
        
        CHECK(registry.find_importer(luisa::string_view{".obj"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".fbx"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".gltf"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".glb"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".stl"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".ply"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".off"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".3ds"}, mesh_type) != nullptr);
        CHECK(registry.find_importer(luisa::string_view{".dae"}, mesh_type) != nullptr);
        // CHECK(registry.find_importer(luisa::string_view{".abc"}, mesh_type) != nullptr);
        // CHECK(registry.find_importer(luisa::string_view{".lwo"}, mesh_type) != nullptr);
        
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
