#include <rbc_core/json_serde.h>
#include <luisa/vstl/common.h>
#include <luisa/core/logging.h>

using namespace luisa;
namespace rbc {
#define JSON_DESER_INIT_ARR                           \
    LUISA_DEBUG_ASSERT(!_json_scope.empty());         \
    auto &v = _json_scope.back();                     \
    yyjson_val *val;                                  \
    if (v.second.is_type_of<ReadArray>()) {           \
        auto &iter = v.second.force_get<ReadArray>(); \
        if (iter.idx >= iter.size) return false;      \
        val = iter.arr_iter;                          \
        if (!val) { return false; }                   \
        iter.arr_iter = unsafe_yyjson_get_next(val);  \
        iter.idx += 1;                                \
    } else {                                          \
        auto &iter = v.second.force_get<ReadObj>();   \
        auto key = yyjson_obj_iter_next(&iter.iter);  \
        if (!key) return false;                       \
        val = yyjson_obj_iter_get_val(key);           \
        if (!val) { return false; }                   \
        iter.last_key = unsafe_yyjson_get_str(key);   \
    }                                                 \
    auto type = yyjson_get_type(val);

#define JSON_DESER_INIT_OBJ                            \
    LUISA_DEBUG_ASSERT(!_json_scope.empty());          \
    auto &v = _json_scope.back();                      \
    if (!v.second.is_type_of<ReadObj>()) return false; \
    auto val = yyjson_obj_get(v.first, name);          \
    if (!val) return false;                            \
    auto type = yyjson_get_type(val);
JsonWriter::JsonWriter(bool root_array) 
: _alloc(4096, &_alloc_callback, 2)
{
    alc = yyjson_alc{
        .malloc = +[](void *, size_t size) { return vengine_malloc(size); }, .realloc = +[](void *, void *ptr, size_t old_size, size_t size) { return vengine_realloc(ptr, size); }, .free = +[](void *, void *ptr) { vengine_free(ptr); }};
    json_doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root;
    if (root_array) {
        root = yyjson_mut_arr(json_doc);
        _json_scope.emplace_back(root, true);
    } else {
        root = yyjson_mut_obj(json_doc);
        _json_scope.emplace_back(root, false);
    }
    yyjson_mut_doc_set_root(json_doc, root);
}
    void *JsonWriter::allocate_temp_str(size_t size) {
        auto c = _alloc.allocate(size);
        return reinterpret_cast<void *>(c.handle + c.offset);
    }

BinaryBlob JsonWriter::write_to() const {
    size_t len{};
    auto json = yyjson_mut_write_opts(json_doc, 0, &alc, &len, nullptr);
    return {
        reinterpret_cast<std::byte *>(json),
        len,
        [](void *ptr) { if(ptr) vengine_free(ptr); }};
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
    auto char_len = strlen(name);
    auto temp_ptr = (char *)allocate_temp_str(char_len + 1);
    temp_ptr[char_len] = 0;
    std::memcpy(temp_ptr, name, char_len);
    LUISA_DEBUG_ASSERT(_json_scope.size() > 1);
    auto obj = _json_scope.back();
    _json_scope.pop_back();
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    LUISA_ASSERT(yyjson_mut_obj_add_val(json_doc, v.first, temp_ptr, obj.first));
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
    LUISA_ASSERT(yyjson_mut_arr_add_sint(json_doc, v.first, int_value));
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
void JsonWriter::clear_alloc() {
    _alloc.clear();
}
void JsonWriter::add(luisa::string_view str) {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(v.second);
    auto ptr = (char*)allocate_temp_str(str.size() + 1);
    ptr[str.size()] = 0;
    std::memcpy(ptr, str.data(), str.size());
    LUISA_ASSERT(yyjson_mut_arr_add_str(json_doc, v.first, ptr));
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
    LUISA_ASSERT(yyjson_mut_obj_add_sint(json_doc, v.first, name, int_value));
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
void JsonWriter::add(luisa::string_view str, char const *name) {
    if (!name) {
        add(str);
        return;
    }
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    LUISA_DEBUG_ASSERT(!v.second);
    auto ptr = (char*)allocate_temp_str(str.size() + 1);
    ptr[str.size()] = 0;
    std::memcpy(ptr, str.data(), str.size());
    LUISA_ASSERT(yyjson_mut_obj_add_str(json_doc, v.first, name, ptr));
}

JsonWriter::~JsonWriter() {
    LUISA_ASSERT(_json_scope.size() == 1);
    yyjson_mut_doc_free(json_doc);
}
JsonReader::JsonReader(luisa::string_view str) {
    alc = yyjson_alc{
        .malloc = +[](void *, size_t size) { return vengine_malloc(size); }, .realloc = +[](void *, void *ptr, size_t old_size, size_t size) { return vengine_realloc(ptr, size); }, .free = +[](void *, void *ptr) { vengine_free(ptr); }};
    json_doc = yyjson_read_opts(const_cast<char *>(str.data()), str.size(), 0, &alc, nullptr);
    if (!json_doc) [[unlikely]] {
        LUISA_ERROR("Bad json doc.");
    }
    auto root_val = json_doc->root;
    auto root_type = yyjson_get_type(root_val);
    if (root_type != YYJSON_TYPE_OBJ && root_type != YYJSON_TYPE_ARR) [[unlikely]] {
        LUISA_ERROR("Bad json format.");
    }
    if (root_type == YYJSON_TYPE_ARR) {
        _json_scope.emplace_back(root_val, ReadArray{unsafe_yyjson_get_first(root_val), unsafe_yyjson_get_len(root_val), 0});
    } else {
        ReadObj obj;
        yyjson_obj_iter_init(root_val, &obj.iter);
        _json_scope.emplace_back(root_val, obj);
    }
}
bool JsonReader::start_array(uint64_t &size, char const *name) {
    if (!name) {
        return start_array(size);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_ARR) return false;
    size = unsafe_yyjson_get_len(val);
    _json_scope.emplace_back(val, ReadArray(unsafe_yyjson_get_first(val), size, 0));
    return true;
}
bool JsonReader::start_object(char const *name) {
    if (!name) {
        return start_object();
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_OBJ) return false;
    ReadObj obj;
    yyjson_obj_iter_init(val, &obj.iter);
    _json_scope.emplace_back(val, obj);
    return true;
}

bool JsonReader::start_array(uint64_t &size) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_ARR) return false;
    size = unsafe_yyjson_get_len(val);
    _json_scope.emplace_back(val, ReadArray(unsafe_yyjson_get_first(val), size, 0));
    return true;
}
bool JsonReader::start_object() {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_OBJ) return false;
    ReadObj obj;
    yyjson_obj_iter_init(val, &obj.iter);
    _json_scope.emplace_back(val, obj);
    return true;
}
void JsonReader::end_scope() {
    LUISA_ASSERT(!_json_scope.empty());
    _json_scope.pop_back();
}
JsonReader::~JsonReader() {
    LUISA_ASSERT(_json_scope.size() == 1);
    yyjson_doc_free(json_doc);
}
bool JsonReader::read(bool &value, char const *name) {
    if (!name) {
        return read(value);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_BOOL) return false;
    value = unsafe_yyjson_get_bool(val);
    return true;
}
bool JsonReader::read(int64_t &value, char const *name) {
    if (!name) {
        return read(value);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_sint(val);
    return true;
}
bool JsonReader::read(uint64_t &value, char const *name) {
    if (!name) {
        return read(value);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_uint(val);
    return true;
}
bool JsonReader::read(double &value, char const *name) {
    if (!name) {
        return read(value);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_num(val);
    return true;
}
bool JsonReader::read(luisa::string &value, char const *name) {
    if (!name) {
        return read(value);
    }
    JSON_DESER_INIT_OBJ
    if (type != YYJSON_TYPE_STR) return false;
    value = unsafe_yyjson_get_str(val);
    return true;
}
bool JsonReader::read(bool &value) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_BOOL) return false;
    value = unsafe_yyjson_get_bool(val);
    return true;
}
bool JsonReader::read(int64_t &value) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_sint(val);
    return true;
}
bool JsonReader::read(uint64_t &value) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_uint(val);
    return true;
}
bool JsonReader::read(double &value) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_NUM) return false;
    value = unsafe_yyjson_get_num(val);
    return true;
}

bool JsonReader::read(luisa::string &value) {
    JSON_DESER_INIT_ARR
    if (type != YYJSON_TYPE_STR) return false;
    value = unsafe_yyjson_get_str(val);
    return true;
}

luisa::string_view JsonReader::last_key() const {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    if (!v.second.is_type_of<ReadObj>()) return "";
    return v.second.force_get<ReadObj>().last_key;
}

uint64_t JsonReader::last_array_size() const {
    LUISA_DEBUG_ASSERT(!_json_scope.empty());
    auto &v = _json_scope.back();
    if (!v.second.is_type_of<ReadArray>()) return 0;
    return v.second.force_get<ReadArray>().size;
}

}// namespace rbc
#undef JSON_DESER_INIT_ARR
#undef JSON_DESER_INIT_OBJ