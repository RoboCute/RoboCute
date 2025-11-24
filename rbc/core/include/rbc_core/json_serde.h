#pragma once
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/core/binary_io.h>
#include <rbc_config.h>
#include <yyjson.h>

namespace rbc {

struct RBC_CORE_API JsonWriter {
    yyjson_alc alc;
    yyjson_mut_doc *json_doc;
    // bool: is_array
    luisa::vector<std::pair<yyjson_mut_val *, bool>> _json_scope;
    JsonWriter(JsonWriter const &) = delete;
    JsonWriter(JsonWriter &&) = delete;

    JsonWriter(bool root_array = false);
    ~JsonWriter();

    void start_array();
    void start_object();
    void add_last_scope_to_object();
    void add_last_scope_to_object(char const *name);
    // array
    void add(bool bool_value);
    void add(int64_t int_value);
    void add(uint64_t uint_value);
    void add(double float_value);
    void add_arr(luisa::span<int64_t const> int_values);
    void add_arr(luisa::span<uint64_t const> uint_values);
    void add_arr(luisa::span<double const> double_values);
    void add_arr(luisa::span<bool const> bool_values);
    void add(char const *str);
    // object
    void add(bool bool_value, char const *name);
    void add(int64_t int_value, char const *name);
    void add(uint64_t uint_value, char const *name);
    void add(double float_value, char const *name);
    void add_arr(luisa::span<int64_t const> int_values, char const *name);
    void add_arr(luisa::span<uint64_t const> uint_values, char const *name);
    void add_arr(luisa::span<double const> double_values, char const *name);
    void add_arr(luisa::span<bool const> bool_values, char const *name);
    void add(char const *str, char const *name);
    luisa::BinaryBlob write_to() const;
};
struct ReadArray {
    yyjson_val *arr_iter;
    uint64_t size;
    uint64_t idx;
};
struct ReadObj {
    luisa::string last_key;
    yyjson_obj_iter iter;
};
struct RBC_CORE_API JsonReader {
    yyjson_alc alc;
    yyjson_doc *json_doc;
    luisa::vector<std::pair<yyjson_val *, vstd::variant<ReadArray, ReadObj>>> _json_scope;
    JsonReader(luisa::string_view str);
    ~JsonReader();

    uint64_t last_array_size() const;
    luisa::string_view last_key() const;
    bool start_array(uint64_t &size);
    bool start_object();
    bool start_array(uint64_t &size, char const *name);
    bool start_object(char const *name);
    void end_scope();
    // array
    bool read(bool &value);
    bool read(int64_t &value);
    bool read(uint64_t &value);
    bool read(double &value);
    bool read(luisa::string &value);
    // obj
    bool read(bool &value, char const *name);
    bool read(int64_t &value, char const *name);
    bool read(uint64_t &value, char const *name);
    bool read(double &value, char const *name);
    bool read(luisa::string &value, char const *name);
};
}// namespace rbc