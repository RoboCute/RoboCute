#include <rbc_core/json_serde.h>
#include <luisa/vstl/common.h>
#include <luisa/core/logging.h>
using namespace luisa;
namespace rbc {
JsonWriter::JsonWriter() {
    alc = yyjson_alc{
        .malloc = +[](void *, size_t size) { return vengine_malloc(size); }, .realloc = +[](void *, void *ptr, size_t old_size, size_t size) { return vengine_realloc(ptr, size); }, .free = +[](void *, void *ptr) { vengine_free(ptr); }};
    json_doc = yyjson_mut_doc_new(&alc);
    auto root = yyjson_mut_obj(json_doc);
    _json_scope.emplace_back(root, false);
    yyjson_mut_doc_set_root(json_doc, root);
}
BinaryBlob JsonWriter::write_to() const {
    size_t len{};
    auto json = yyjson_mut_write_opts(json_doc, 0, &alc, &len, nullptr);
    return {
        reinterpret_cast<std::byte *>(json),
        len,
        [](void *ptr) { vengine_free(ptr); }};
}
void JsonWriter::start_array() {
    _json_scope.emplace_back(yyjson_mut_arr(json_doc), true);
}
void JsonWriter::start_object() {
    _json_scope.emplace_back(yyjson_mut_obj(json_doc), false);
}
void JsonWriter::add_last_scope_to_object() {
    auto obj = _json_scope.back();
    _json_scope.pop_back();
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_val(v.first, obj.first));
}
void JsonWriter::add_last_scope_to_object(char const *name) {
    if (!name) {
        add_last_scope_to_object();
        return;
    }
    LUISA_DEBUG_ASSERT(_json_scope.size() > 1);
    auto obj = _json_scope.back();
    _json_scope.pop_back();
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, name, obj.first));
}

void JsonWriter::add(bool bool_value) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_bool(json_doc, v.first, bool_value));
}
void JsonWriter::add(int64_t int_value) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_int(json_doc, v.first, int_value));
}
void JsonWriter::add(uint64_t uint_value) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_uint(json_doc, v.first, uint_value));
}
void JsonWriter::add(double float_value) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_double(json_doc, v.first, float_value));
}
void JsonWriter::add_arr(luisa::span<int64_t const> int_values) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_val(v.first, yyjson_mut_arr_with_sint64(json_doc, int_values.data(), int_values.size())));
}
void JsonWriter::add_arr(luisa::span<uint64_t const> uint_values) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_val(v.first, yyjson_mut_arr_with_uint64(json_doc, uint_values.data(), uint_values.size())));
}
void JsonWriter::add_arr(luisa::span<double const> double_values) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_val(v.first, yyjson_mut_arr_with_double(json_doc, double_values.data(), double_values.size())));
}
void JsonWriter::add_arr(luisa::span<bool const> bool_values) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_val(v.first, yyjson_mut_arr_with_bool(json_doc, bool_values.data(), bool_values.size())));
}

void JsonWriter::add(char const *str) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    LUISA_ASSERT(yyjson_mut_arr_add_str(json_doc, v.first, str));
}

void JsonWriter::add(bool bool_value, char const *name) {
    if (!name) {
        add(bool_value);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_bool(json_doc, v.first, name, bool_value));
}
void JsonWriter::add(int64_t int_value, char const *name) {
    if (!name) {
        add(int_value);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_int(json_doc, v.first, name, int_value));
}
void JsonWriter::add(uint64_t uint_value, char const *name) {
    if (!name) {
        add(uint_value);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_uint(json_doc, v.first, name, uint_value));
}
void JsonWriter::add(double float_value, char const *name) {
    if (!name) {
        add(float_value);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_double(json_doc, v.first, name, float_value));
}
void JsonWriter::add_arr(luisa::span<int64_t const> int_values, char const *name) {
    if (!name) {
        add_arr(int_values);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, name, yyjson_mut_arr_with_sint64(json_doc, int_values.data(), int_values.size())));
}
void JsonWriter::add_arr(luisa::span<uint64_t const> uint_values, char const *name) {
    if (!name) {
        add_arr(uint_values);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, name, yyjson_mut_arr_with_uint64(json_doc, uint_values.data(), uint_values.size())));
}
void JsonWriter::add_arr(luisa::span<double const> double_values, char const *name) {
    if (!name) {
        add_arr(double_values);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, name, yyjson_mut_arr_with_double(json_doc, double_values.data(), double_values.size())));
}
void JsonWriter::add_arr(luisa::span<bool const> bool_values, char const *name) {
    if (!name) {
        add_arr(bool_values);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, name, yyjson_mut_arr_with_bool(json_doc, bool_values.data(), bool_values.size())));
}
void JsonWriter::add(char const *str, char const *name) {
    if (!name) {
        add(str);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_str(json_doc, v.first, name, str));
}

JsonWriter::~JsonWriter() {
    LUISA_ASSERT(_json_scope.size() == 1);
    yyjson_mut_doc_free(json_doc);
}
}// namespace rbc