#include "rbc_test.hpp"

// Define SKIP_IF for doctest versions that don't have it
#ifndef SKIP_IF
#define SKIP_IF(cond) \
    if (cond) { return; }
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
    // ut framework: no command-line filtering by default; run all importers.
    (void)type;
    return true;
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

namespace rbc::test {

suite<"World|Model"> WorldModelTestSuite = [] {

    "mesh_importer_obj"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("obj"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".obj"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".obj"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        // Test importing non-existent file returns false
        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.obj");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_fbx"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("fbx"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".fbx"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".fbx"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.fbx");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_gltf"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("gltf"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".gltf"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".gltf"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.gltf");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_glb"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("glb"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".glb"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".glb"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.glb");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_stl"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("stl"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".stl"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".stl"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.stl");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_ply"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("ply"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".ply"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".ply"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.ply");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_off"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("off"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".off"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".off"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.off");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_3ds"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("3ds"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".3ds"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".3ds"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.3ds");
        expect(static_cast<bool>(result));
    };

    "mesh_importer_collada"_test = [] {
        WorldFixture __ut_fixture;
        auto &self = __ut_fixture;
        SKIP_IF(!should_test_model_type("dae"));
        auto &registry = ResourceImporterRegistry::instance();
        auto importer = registry.find_importer(luisa::string_view{".dae"}, TypeInfo::get<rbc::world::MeshResource>().md5());
        expect(static_cast<bool>(importer != nullptr));
        expect(static_cast<bool>(importer->extension() == ".dae"));
        expect(static_cast<bool>(importer->resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

        auto mesh = self.create<MeshResource>();
        auto result = importer->import(mesh, "test_model.dae");
        expect(static_cast<bool>(result));
    };

    // "mesh_importer_abc"_test = [] {
    //     WorldFixture __ut_fixture;
    //     auto &self = __ut_fixture;
    //     AbcMeshImporter importer;
    //     expect(static_cast<bool>(importer.extension() == ".abc"));
    //     expect(static_cast<bool>(importer.resource_type() == TypeInfo::get<rbc::world::MeshResource>().md5()));

    //     auto mesh = self.create<MeshResource>();
    //     auto result = importer.import(mesh, "test_model.abc");
    //     expect(!static_cast<bool>(result));
    // }

    "importer_registry"_test = [] {
        WorldFixture self;
        auto &registry = ResourceImporterRegistry::instance();

        // Test finding importers by extension
        auto mesh_type = TypeInfo::get<rbc::world::MeshResource>().md5();

        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".obj"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".fbx"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".gltf"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".glb"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".stl"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".ply"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".off"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".3ds"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".dae"}, mesh_type) != nullptr));
        // expect(static_cast<bool>(registry.find_importer(luisa::string_view{".abc"}, mesh_type) != nullptr));
        // expect(static_cast<bool>(registry.find_importer(luisa::string_view{".lwo"}, mesh_type) != nullptr));

        // Test case insensitivity for extensions
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".OBJ"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".Fbx"}, mesh_type) != nullptr));
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".GLTF"}, mesh_type) != nullptr));

        // Test non-existent extension returns nullptr
        expect(static_cast<bool>(registry.find_importer(luisa::string_view{".nonexistent"}, mesh_type) == nullptr));
    };

    "mesh_resource_creation"_test = [] {
        WorldFixture self;
        auto mesh = self.create<MeshResource>();
        expect(static_cast<bool>(mesh != nullptr));
        expect(static_cast<bool>(mesh->empty()));
        expect(static_cast<bool>(mesh->vertex_count() == 0));
        expect(static_cast<bool>(mesh->triangle_count() == 0));
        expect(static_cast<bool>(mesh->uv_count() == 0));
        expect(!static_cast<bool>(mesh->contained_normal()));
        expect(!static_cast<bool>(mesh->contained_tangent()));
    };
}; // suite

} // namespace rbc::test
