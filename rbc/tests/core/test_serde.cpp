#include "test_util.h"
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <rbc_core/serde.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/bin_serde.h>
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

TEST_SUITE("serde") {
    TEST_CASE("json_basic_struct_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        test_serde::TestPoint point;
        point.x = 1.5;
        point.y = 2.5;
        point.z = 3.5;

        JsonSerializer writer;
        writer._store(point, "test_point");
        auto json_blob = writer.write_to();

        JsonDeSerializer reader{luisa::string_view{(char const *)json_blob.data(), json_blob.size()}};
        test_serde::TestPoint deserialized_point;
        CHECK(reader._load(deserialized_point, "test_point"));

        CHECK(deserialized_point.x == point.x);
        CHECK(deserialized_point.y == point.y);
        CHECK(deserialized_point.z == point.z);
    }

    TEST_CASE("json_nested_struct_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

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

        JsonDeSerializer reader{luisa::string_view{(char const *)json_blob.data(), json_blob.size()}};
        test_serde::TestTransform deserialized_transform;
        CHECK(reader._load(deserialized_transform, "transform"));

        CHECK(deserialized_transform.position.x == transform.position.x);
        CHECK(deserialized_transform.position.y == transform.position.y);
        CHECK(deserialized_transform.position.z == transform.position.z);
        CHECK(deserialized_transform.rotation.x == transform.rotation.x);
        CHECK(deserialized_transform.rotation.y == transform.rotation.y);
        CHECK(deserialized_transform.rotation.z == transform.rotation.z);
        CHECK(deserialized_transform.scale.x == transform.scale.x);
        CHECK(deserialized_transform.scale.y == transform.scale.y);
        CHECK(deserialized_transform.scale.z == transform.scale.z);
        CHECK(deserialized_transform.color.r == transform.color.r);
        CHECK(deserialized_transform.color.g == transform.color.g);
        CHECK(deserialized_transform.color.b == transform.color.b);
        CHECK(deserialized_transform.color.a == transform.color.a);
    }

    TEST_CASE("json_array_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

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

        JsonDeSerializer reader{luisa::string_view{(char const *)json_blob.data(), json_blob.size()}};
        test_serde::TestArrayStruct deserialized_array;
        CHECK(reader._load(deserialized_array, "array_struct"));

        CHECK(deserialized_array.int_array.size() == array_struct.int_array.size());
        for (size_t i = 0; i < array_struct.int_array.size(); ++i) {
            CHECK(deserialized_array.int_array[i] == array_struct.int_array[i]);
        }

        CHECK(deserialized_array.string_array.size() == array_struct.string_array.size());
        for (size_t i = 0; i < array_struct.string_array.size(); ++i) {
            CHECK(deserialized_array.string_array[i] == array_struct.string_array[i]);
        }

        CHECK(deserialized_array.point_array.size() == array_struct.point_array.size());
        for (size_t i = 0; i < array_struct.point_array.size(); ++i) {
            CHECK(deserialized_array.point_array[i].x == array_struct.point_array[i].x);
            CHECK(deserialized_array.point_array[i].y == array_struct.point_array[i].y);
            CHECK(deserialized_array.point_array[i].z == array_struct.point_array[i].z);
        }
    }

    TEST_CASE("json_array_root_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        luisa::vector<test_serde::TestPoint> points;
        points.emplace_back(test_serde::TestPoint{1.0, 2.0, 3.0});
        points.emplace_back(test_serde::TestPoint{4.0, 5.0, 6.0});
        points.emplace_back(test_serde::TestPoint{7.0, 8.0, 9.0});

        JsonSerializer writer(true);// root_array = true
        for (const auto &point : points) {
            writer._store(point);
        }
        auto json_blob = writer.write_to();

        JsonDeSerializer reader{luisa::string_view{(char const *)json_blob.data(), json_blob.size()}};
        luisa::vector<test_serde::TestPoint> deserialized_points;
        uint64_t size = reader.last_array_size();
        deserialized_points.reserve(size);
        for (uint64_t i = 0; i < size; ++i) {
            test_serde::TestPoint point;
            CHECK(reader._load(point));
            deserialized_points.emplace_back(point);
        }

        CHECK(deserialized_points.size() == points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            CHECK(deserialized_points[i].x == points[i].x);
            CHECK(deserialized_points[i].y == points[i].y);
            CHECK(deserialized_points[i].z == points[i].z);
        }
    }

    TEST_CASE("json_builtin_types_compatibility") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        JsonSerializer writer;
        writer._store(42, "int_value");
        writer._store(3.14, "double_value");
        writer._store(luisa::string_view("test_string"), "string_value");
        writer._store(true, "bool_value");
        auto json_blob = writer.write_to();

        JsonDeSerializer reader{luisa::string_view{(char const *)json_blob.data(), json_blob.size()}};
        int32_t int_val;
        double double_val;
        luisa::string string_val;
        bool bool_val;

        CHECK(reader._load(int_val, "int_value"));
        CHECK(reader._load(double_val, "double_value"));
        CHECK(reader._load(string_val, "string_value"));
        CHECK(reader._load(bool_val, "bool_value"));

        CHECK(int_val == 42);
        CHECK(double_val == 3.14);
        CHECK(string_val == "test_string");
        CHECK(bool_val == true);
    }

    TEST_CASE("bin_basic_struct_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        test_serde::TestPoint point;
        point.x = 1.5;
        point.y = 2.5;
        point.z = 3.5;

        BinSerializer writer;
        writer._store(point, "test_point");
        auto bin_blob = writer.write_to();

        BinDeSerializer reader{bin_blob};
        test_serde::TestPoint deserialized_point;
        CHECK(reader._load(deserialized_point, "test_point"));

        CHECK(deserialized_point.x == point.x);
        CHECK(deserialized_point.y == point.y);
        CHECK(deserialized_point.z == point.z);
    }

    TEST_CASE("bin_nested_struct_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

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

        BinSerializer writer;
        writer._store(transform, "transform");
        auto bin_blob = writer.write_to();

        BinDeSerializer reader{bin_blob};
        test_serde::TestTransform deserialized_transform;
        CHECK(reader._load(deserialized_transform, "transform"));

        CHECK(deserialized_transform.position.x == transform.position.x);
        CHECK(deserialized_transform.position.y == transform.position.y);
        CHECK(deserialized_transform.position.z == transform.position.z);
        CHECK(deserialized_transform.rotation.x == transform.rotation.x);
        CHECK(deserialized_transform.rotation.y == transform.rotation.y);
        CHECK(deserialized_transform.rotation.z == transform.rotation.z);
        CHECK(deserialized_transform.scale.x == transform.scale.x);
        CHECK(deserialized_transform.scale.y == transform.scale.y);
        CHECK(deserialized_transform.scale.z == transform.scale.z);
        CHECK(deserialized_transform.color.r == transform.color.r);
        CHECK(deserialized_transform.color.g == transform.color.g);
        CHECK(deserialized_transform.color.b == transform.color.b);
        CHECK(deserialized_transform.color.a == transform.color.a);
    }

    TEST_CASE("bin_bytes_interface") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        luisa::vector<std::byte> test_bytes;
        test_bytes.emplace_back(std::byte{0x01});
        test_bytes.emplace_back(std::byte{0x02});
        test_bytes.emplace_back(std::byte{0x03});
        test_bytes.emplace_back(std::byte{0xFF});

        BinSerializer writer;
        writer.bytes(luisa::span<std::byte const>{test_bytes.data(), test_bytes.size()}, "test_bytes");
        auto bin_blob = writer.write_to();

        BinDeSerializer reader{bin_blob};
        luisa::vector<std::byte> deserialized_bytes;
        CHECK(reader.bytes(deserialized_bytes, "test_bytes"));

        CHECK(deserialized_bytes.size() == test_bytes.size());
        for (size_t i = 0; i < test_bytes.size(); ++i) {
            CHECK(deserialized_bytes[i] == test_bytes[i]);
        }
    }

    TEST_CASE("bin_array_root_serialization") {
        RuntimeStaticBase::init_all();
        auto dispose_runtime_static = vstd::scope_exit([] {
            RuntimeStaticBase::dispose_all();
        });

        luisa::vector<test_serde::TestPoint> points;
        points.emplace_back(test_serde::TestPoint{1.0, 2.0, 3.0});
        points.emplace_back(test_serde::TestPoint{4.0, 5.0, 6.0});
        points.emplace_back(test_serde::TestPoint{7.0, 8.0, 9.0});

        BinSerializer writer(true);// root_array = true
        for (const auto &point : points) {
            writer._store(point);
        }
        auto bin_blob = writer.write_to();

        BinDeSerializer reader{bin_blob};
        luisa::vector<test_serde::TestPoint> deserialized_points;
        uint64_t size = reader.last_array_size();
        deserialized_points.reserve(size);
        for (uint64_t i = 0; i < size; ++i) {
            test_serde::TestPoint point;
            CHECK(reader._load(point));
            deserialized_points.emplace_back(point);
        }

        CHECK(deserialized_points.size() == points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            CHECK(deserialized_points[i].x == points[i].x);
            CHECK(deserialized_points[i].y == points[i].y);
            CHECK(deserialized_points[i].z == points[i].z);
        }
    }
}
