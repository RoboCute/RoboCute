#pragma once
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>
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

    JsonWriter();
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
// struct RBC_CORE_API JsonReader {
//     luisa::vector<std::pair<void *, bool>> _json_scope;
//     void *json_doc;
//     JsonReader();
//     ~JsonReader();
// private:
//     void start_object(char const *name);
//     void end_scope();
// };
}// namespace rbc