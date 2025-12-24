#include "generated/generated.hpp"
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <luisa/core/magic_enum.h>
#include <rbc_core/serde.h>
#include <rbc_core/type_info.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/state_map.h>
#include <luisa/vstl/md5.h>
#include <rbc_core/func_serializer.h>
#include <rbc_runtime/plugin_manager.h>
#include <rbc_world/base_object.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/entity.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resource_base.h>
#include <rbc_core/runtime_static.h>

using namespace rbc;
using namespace luisa;

// Test structures for Serialize<T> template specialization
namespace test_serde {
struct TestPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct TestColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct TestTransform {
    TestPoint position;
    TestPoint rotation;
    TestPoint scale{1.0, 1.0, 1.0};
    TestColor color;
};

struct TestArrayStruct {
    luisa::vector<int32_t> int_array;
    luisa::vector<luisa::string> string_array;
    luisa::vector<TestPoint> point_array;
};
}// namespace test_serde

// Serialize<T> specializations for test types
namespace rbc {
template<>
struct Serialize<test_serde::TestPoint> {
    static void write(ArchiveWrite &w, const test_serde::TestPoint &v) {
        w.value(v.x, "x");
        w.value(v.y, "y");
        w.value(v.z, "z");
    }

    static bool read(ArchiveRead &r, test_serde::TestPoint &v) {
        if (!r.value(v.x, "x")) return false;
        if (!r.value(v.y, "y")) return false;
        if (!r.value(v.z, "z")) return false;
        return true;
    }
};

template<>
struct Serialize<test_serde::TestColor> {
    static void write(ArchiveWrite &w, const test_serde::TestColor &v) {
        w.value(v.r, "r");
        w.value(v.g, "g");
        w.value(v.b, "b");
        w.value(v.a, "a");
    }

    static bool read(ArchiveRead &r, test_serde::TestColor &v) {
        if (!r.value(v.r, "r")) return false;
        if (!r.value(v.g, "g")) return false;
        if (!r.value(v.b, "b")) return false;
        if (!r.value(v.a, "a")) return false;
        return true;
    }
};

template<>
struct Serialize<test_serde::TestTransform> {
    static void write(ArchiveWrite &w, const test_serde::TestTransform &v) {
        w.value(v.position, "position");
        w.value(v.rotation, "rotation");
        w.value(v.scale, "scale");
        w.value(v.color, "color");
    }

    static bool read(ArchiveRead &r, test_serde::TestTransform &v) {
        if (!r.value(v.position, "position")) return false;
        if (!r.value(v.rotation, "rotation")) return false;
        if (!r.value(v.scale, "scale")) return false;
        if (!r.value(v.color, "color")) return false;
        return true;
    }
};

template<>
struct Serialize<test_serde::TestArrayStruct> {
    static void write(ArchiveWrite &w, const test_serde::TestArrayStruct &v) {
        w.value(v.int_array, "int_array");
        w.value(v.string_array, "string_array");
        w.value(v.point_array, "point_array");
    }

    static bool read(ArchiveRead &r, test_serde::TestArrayStruct &v) {
        if (!r.value(v.int_array, "int_array")) return false;
        if (!r.value(v.string_array, "string_array")) return false;
        if (!r.value(v.point_array, "point_array")) return false;
        return true;
    }
};

}// namespace rbc

int main() {
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        world::dispose_resource_loader();
        RuntimeStaticBase::dispose_all();
    });
    world::init_world();
    auto dsp_world = vstd::scope_exit([&]() {
        world::destroy_world();
    });

    // ====== Test Raw API =============
    LUISA_INFO("=== Testing Raw API ===");
    luisa::BinaryBlob json;
    {
        auto entity = world::create_object_with_guid<world::Entity>(vstd::Guid(true));

        auto trans = entity->add_component<world::TransformComponent>();
        trans->set_pos(double3(114, 514, 1919), false);
        JsonSerializer writer(true);
        world::ObjSerialize writer_args(writer);
        // serialize entity and components
        auto guid = entity->guid();
        auto type_id = entity->type_id();
        writer.start_object();
        if (guid) {
            writer._store(guid, "__guid__");
            entity->serialize_meta(writer_args);
        }
        writer.add_last_scope_to_object();
        json = writer.write_to();
        auto trans_ptr = entity->get_component(TypeInfo::get<world::TransformComponent>());
        // test life time
        LUISA_ASSERT(trans_ptr == trans);
        trans->dispose();
        trans_ptr = entity->get_component(TypeInfo::get<world::TransformComponent>());
        // tarnsform already destroyed
        LUISA_ASSERT(trans_ptr == nullptr);
        entity->dispose();
    }
    // Try deserialize
    auto json_str = luisa::string_view{
        (char const *)json.data(),
        json.size()};
    LUISA_INFO("{}", json_str);
    rbc::JsonDeSerializer reader{json_str};
    auto entity_size = reader.last_array_size();
    luisa::vector<world::Entity *> entities;
    entities.reserve(entity_size);
    for (auto i : vstd::range(entity_size)) {
        reader.start_object();
        vstd::Guid guid;
        LUISA_ASSERT(reader._load(guid, "__guid__"));
        auto entity = entities.emplace_back(world::create_object_with_guid<world::Entity>(guid));
        auto deser_obj = world::ObjDeSerialize{.ser = reader};
        entity->deserialize_meta(deser_obj);
        reader.end_scope();
    }
    for (auto &i : entities) {
        auto trans = i->get_component<world::TransformComponent>();
        LUISA_INFO("{}", trans->position());
    }
    // Dispose all entities created during deserialization to prevent leaks
    for (auto &i : entities) {
        i->dispose();
    }
    LUISA_INFO("=== Testing Raw API Done ===");

    // Test Serialize<T> template specialization API
    LUISA_INFO("=== Testing Serialize<T> template specialization ===");
    // Test 1: Basic struct serialization
    {
        test_serde::TestPoint point;
        point.x = 1.5;
        point.y = 2.5;
        point.z = 3.5;

        JsonSerializer writer;
        writer._store(point, "test_point");
        auto json_blob = writer.write_to();

        luisa::string_view json_str{(char const *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized TestPoint: {}", json_str);

        JsonDeSerializer reader{json_str};
        test_serde::TestPoint deserialized_point;
        LUISA_ASSERT(reader._load(deserialized_point, "test_point"));

        LUISA_ASSERT(deserialized_point.x == point.x);
        LUISA_ASSERT(deserialized_point.y == point.y);
        LUISA_ASSERT(deserialized_point.z == point.z);
        LUISA_INFO("TestPoint serialization/deserialization: PASSED");
    }

    // Test 2: Nested struct serialization
    {
        test_serde::TestTransform transform;
        transform.position.x = 10.0;
        transform.position.y = 20.0;
        transform.position.z = 30.0;
        transform.rotation.x = 45.0;
        transform.rotation.y = 90.0;
        transform.rotation.z = 180.0;
        transform.scale.x = 2.0;
        transform.scale.y = 2.0;
        transform.scale.z = 2.0;
        transform.color.r = 1.0f;
        transform.color.g = 0.5f;
        transform.color.b = 0.25f;
        transform.color.a = 1.0f;

        JsonSerializer writer;
        writer._store(transform, "transform");
        auto json_blob = writer.write_to();

        luisa::string_view json_str{(char const *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized TestTransform: {}", json_str);

        JsonDeSerializer reader{json_str};
        test_serde::TestTransform deserialized_transform;
        LUISA_ASSERT(reader._load(deserialized_transform, "transform"));

        LUISA_ASSERT(deserialized_transform.position.x == transform.position.x);
        LUISA_ASSERT(deserialized_transform.position.y == transform.position.y);
        LUISA_ASSERT(deserialized_transform.position.z == transform.position.z);
        LUISA_ASSERT(deserialized_transform.rotation.x == transform.rotation.x);
        LUISA_ASSERT(deserialized_transform.rotation.y == transform.rotation.y);
        LUISA_ASSERT(deserialized_transform.rotation.z == transform.rotation.z);
        LUISA_ASSERT(deserialized_transform.scale.x == transform.scale.x);
        LUISA_ASSERT(deserialized_transform.scale.y == transform.scale.y);
        LUISA_ASSERT(deserialized_transform.scale.z == transform.scale.z);
        LUISA_ASSERT(deserialized_transform.color.r == transform.color.r);
        LUISA_ASSERT(deserialized_transform.color.g == transform.color.g);
        LUISA_ASSERT(deserialized_transform.color.b == transform.color.b);
        LUISA_ASSERT(deserialized_transform.color.a == transform.color.a);
        LUISA_INFO("TestTransform serialization/deserialization: PASSED");
    }

    // Test 3: Array serialization
    {
        test_serde::TestArrayStruct array_struct;
        array_struct.int_array.emplace_back(1);
        array_struct.int_array.emplace_back(2);
        array_struct.int_array.emplace_back(3);
        array_struct.string_array.emplace_back("hello");
        array_struct.string_array.emplace_back("world");
        array_struct.point_array.emplace_back(test_serde::TestPoint{1.0, 2.0, 3.0});
        array_struct.point_array.emplace_back(test_serde::TestPoint{4.0, 5.0, 6.0});

        JsonSerializer writer;
        writer._store(array_struct, "array_struct");
        auto json_blob = writer.write_to();

        luisa::string_view json_str{(char const *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized TestArrayStruct: {}", json_str);

        JsonDeSerializer reader{json_str};
        test_serde::TestArrayStruct deserialized_array;
        LUISA_ASSERT(reader._load(deserialized_array, "array_struct"));

        LUISA_ASSERT(deserialized_array.int_array.size() == array_struct.int_array.size());
        for (size_t i = 0; i < array_struct.int_array.size(); ++i) {
            LUISA_ASSERT(deserialized_array.int_array[i] == array_struct.int_array[i]);
        }

        LUISA_ASSERT(deserialized_array.string_array.size() == array_struct.string_array.size());
        for (size_t i = 0; i < array_struct.string_array.size(); ++i) {
            LUISA_ASSERT(deserialized_array.string_array[i] == array_struct.string_array[i]);
        }

        LUISA_ASSERT(deserialized_array.point_array.size() == array_struct.point_array.size());
        for (size_t i = 0; i < array_struct.point_array.size(); ++i) {
            LUISA_ASSERT(deserialized_array.point_array[i].x == array_struct.point_array[i].x);
            LUISA_ASSERT(deserialized_array.point_array[i].y == array_struct.point_array[i].y);
            LUISA_ASSERT(deserialized_array.point_array[i].z == array_struct.point_array[i].z);
        }
        LUISA_INFO("TestArrayStruct serialization/deserialization: PASSED");
    }

    // Test 4: Array root serialization
    {
        luisa::vector<test_serde::TestPoint> points;
        points.emplace_back(test_serde::TestPoint{1.0, 2.0, 3.0});
        points.emplace_back(test_serde::TestPoint{4.0, 5.0, 6.0});
        points.emplace_back(test_serde::TestPoint{7.0, 8.0, 9.0});

        JsonSerializer writer(true);// root_array = true
        for (const auto &point : points) {
            writer._store(point);
        }
        auto json_blob = writer.write_to();

        luisa::string_view json_str{(char const *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized array root: {}", json_str);

        JsonDeSerializer reader{json_str};
        luisa::vector<test_serde::TestPoint> deserialized_points;
        uint64_t size = reader.last_array_size();
        deserialized_points.reserve(size);
        for (uint64_t i = 0; i < size; ++i) {
            test_serde::TestPoint point;
            LUISA_ASSERT(reader._load(point));
            deserialized_points.emplace_back(point);
        }

        LUISA_ASSERT(deserialized_points.size() == points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            LUISA_ASSERT(deserialized_points[i].x == points[i].x);
            LUISA_ASSERT(deserialized_points[i].y == points[i].y);
            LUISA_ASSERT(deserialized_points[i].z == points[i].z);
        }
        LUISA_INFO("Array root serialization/deserialization: PASSED");
    }

    // Test 5: Compatibility with built-in types
    {
        JsonSerializer writer;
        writer._store(42, "int_value");
        writer._store(3.14, "double_value");
        writer._store(luisa::string_view("test_string"), "string_value");
        writer._store(true, "bool_value");
        auto json_blob = writer.write_to();

        luisa::string_view json_str{(char const *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized built-in types: {}", json_str);

        JsonDeSerializer reader{json_str};
        int32_t int_val;
        double double_val;
        luisa::string string_val;
        bool bool_val;

        LUISA_ASSERT(reader._load(int_val, "int_value"));
        LUISA_ASSERT(reader._load(double_val, "double_value"));
        LUISA_ASSERT(reader._load(string_val, "string_value"));
        LUISA_ASSERT(reader._load(bool_val, "bool_value"));

        LUISA_ASSERT(int_val == 42);
        LUISA_ASSERT(double_val == 3.14);
        LUISA_ASSERT(string_val == "test_string");
        LUISA_ASSERT(bool_val == true);
        LUISA_INFO("Built-in types compatibility: PASSED");
    }

    LUISA_INFO("=== All Serialize<T> tests PASSED ===");

    return 0;
    //     rbc::JsonSerializer writer;
    //     StateMap state_map;
    //     {
    //         MyStruct my_struct;
    //         my_struct.a = 114;
    //         my_struct.b = make_double2(1.5, 6.6);
    //         my_struct.c = 514.f;
    //         my_struct.matrix = make_float4x4(
    //             1, 2, 3, 4,
    //             5, 6, 7, 8,
    //             9, 10, 11, 12,
    //             13, 14, 15, 16);
    //         my_struct.dd = "this is string";
    //         my_struct.ee.emplace_back(1919);
    //         my_struct.ee.emplace_back(810);
    //         my_struct.ff.try_emplace("key", make_float4(4, 3, 2, 1));
    //         my_struct.vec_str.emplace_back("str0");
    //         my_struct.vec_str.emplace_back("str1");
    //         my_struct.test_enum = MyEnum::Off;
    //         my_struct.guid.remake();
    //         auto &v0 = my_struct.multi_dim_vec.emplace_back();
    //         v0.emplace_back(1);
    //         v0.emplace_back(2);
    //         auto &v1 = my_struct.multi_dim_vec.emplace_back();
    //         v1.emplace_back(3);
    //         v1.emplace_back(4);
    //         state_map.write_atomic(std::move(my_struct));
    //     }
    //     // Ser
    //     auto text = state_map.serialize_to_json();
    //     auto type = TypeInfo::get<MyStruct>();
    //     LUISA_INFO("Type {} md5 {} json-write {}",
    //                type.name(),
    //                type.md5_to_string(),
    //                (char const *)text.data());

    //     //     // Deser
    //     {
    //         StateMap deser_map;
    //         deser_map.init_json({reinterpret_cast<char const *>(text.data()), text.size()});
    //         auto &&new_struct = deser_map.read<MyStruct>();
    // #define PRINT_MY_STRUCT(m) LUISA_INFO("{} {}", #m, new_struct.m)
    //         PRINT_MY_STRUCT(a);
    //         PRINT_MY_STRUCT(b);
    //         PRINT_MY_STRUCT(c);
    //         PRINT_MY_STRUCT(dd);
    //         PRINT_MY_STRUCT(matrix);
    //         for (auto &i : new_struct.ee) {
    //             LUISA_INFO("ee {}", i);
    //         }
    //         for (auto &i : new_struct.ff) {
    //             LUISA_INFO("ff {}, {}", i.first, i.second);
    //         }
    //         for (auto &i : new_struct.vec_str) {
    //             LUISA_INFO("vec str {}", i);
    //         }
    //         for (auto &i : new_struct.multi_dim_vec) {
    //             for (auto &j : i) {
    //                 LUISA_INFO("multi_dim_vec {}", j);
    //             }
    //         }
    //         LUISA_INFO("test_enum {}", luisa::to_string(new_struct.test_enum));
    //         LUISA_INFO("guid {}", new_struct.guid.to_base64());
    //     }
}