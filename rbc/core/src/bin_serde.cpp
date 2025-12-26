#include "rbc_core/bin_serde.h"
#include <luisa/core/logging.h>
#include <cstring>

namespace rbc {

// BinWriter implementation
BinWriter::BinWriter(bool root_array) : pos_(0) {
    if (root_array) {
        BinScope scope;
        scope.is_array = true;
        scope.array_size = 0;
        scope.array_index = 0;
        scope.in_object = false;
        scope.object_start_written = false;
        scope.object_start_pos = 0;
        write_type(BinType::ArrayStart);
        scope.array_size_pos = buffer_.size();// Save position where size will be written
        write_uint64(0);                      // placeholder for size
        _scope.emplace_back(scope);
    } else {
        BinScope scope;
        scope.is_array = false;
        scope.in_object = true;
        scope.object_start_written = true;
        scope.object_start_pos = 0;
        scope.array_size_pos = 0;
        _scope.emplace_back(scope);
        write_type(BinType::ObjectStart);
    }
}

BinWriter::~BinWriter() {
    LUISA_ASSERT(_scope.size() == 1);
}

void BinWriter::write_type(BinType type) {
    write_bytes(&type, sizeof(BinType));
}

void BinWriter::write_uint64(uint64_t value) {
    write_bytes(&value, sizeof(uint64_t));
}

void BinWriter::write_bytes(void const *data, uint64_t size) {
    if (size == 0) return;
    auto old_size = buffer_.size();
    buffer_.resize(old_size + size);
    std::memcpy(buffer_.data() + old_size, data, size);
}

void BinWriter::start_array() {
    LUISA_DEBUG_ASSERT(!_scope.empty());

    BinScope new_scope;
    new_scope.is_array = true;
    new_scope.array_size = 0;
    new_scope.array_index = 0;
    new_scope.in_object = false;
    new_scope.object_start_written = false;
    new_scope.object_start_pos = 0;
    write_type(BinType::ArrayStart);
    new_scope.array_size_pos = buffer_.size();// Save position where size will be written
    write_uint64(0);                          // placeholder for size
    _scope.emplace_back(new_scope);
}

void BinWriter::start_object() {
    LUISA_DEBUG_ASSERT(!_scope.empty());

    auto &parent = _scope.back();
    BinScope new_scope;
    new_scope.is_array = false;
    new_scope.in_object = true;

    // If we're in an object context, delay writing ObjectStart until we know if there's a key
    if (!parent.is_array && parent.in_object) {
        new_scope.object_start_written = false;
        new_scope.object_start_pos = buffer_.size();
    } else {
        new_scope.object_start_written = true;
        new_scope.object_start_pos = 0;
        write_type(BinType::ObjectStart);
    }

    _scope.emplace_back(new_scope);
}

void BinWriter::add_last_scope_to_object() {
    LUISA_DEBUG_ASSERT(_scope.size() > 1);
    auto scope = _scope.back();
    _scope.pop_back();

    auto &parent = _scope.back();
    LUISA_DEBUG_ASSERT(parent.is_array);

    // Update array size
    parent.array_size++;

    // Write scope end
    write_type(BinType::ScopeEnd);

    // If the scope being added was an array, update its size in the buffer
    // Note: For root array, the size will be updated in write_to()
    if (scope.is_array && scope.array_size_pos > 0 && _scope.size() > 0) {
        // Update the array size at the saved position
        std::memcpy(buffer_.data() + scope.array_size_pos, &scope.array_size, sizeof(uint64_t));
    }
}

void BinWriter::add_last_scope_to_object(char const *name) {
    if (!name) {
        add_last_scope_to_object();
        return;
    }

    LUISA_DEBUG_ASSERT(_scope.size() > 1);
    auto scope = _scope.back();
    _scope.pop_back();

    auto &parent = _scope.back();
    LUISA_DEBUG_ASSERT(!parent.is_array && parent.in_object);

    // If ObjectStart hasn't been written yet, write key first, then ObjectStart
    if (!scope.object_start_written) {
        // Save current buffer size (where we'll insert key + ObjectStart)
        uint64_t insert_pos = scope.object_start_pos;
        uint64_t content_size = buffer_.size() - insert_pos;

        // Save the content that was written after start_object()
        luisa::vector<std::byte> content_backup;
        if (content_size > 0) {
            content_backup.resize(content_size);
            std::memcpy(content_backup.data(), buffer_.data() + insert_pos, content_size);
        }

        // Truncate buffer to insert position
        buffer_.resize(insert_pos);

        // Write key first
        luisa::string_view key(name);
        write_type(BinType::String);
        write_uint64(key.size());
        write_bytes(key.data(), key.size());

        // Write ObjectStart
        write_type(BinType::ObjectStart);

        // Restore the content after ObjectStart
        if (content_size > 0) {
            write_bytes(content_backup.data(), content_size);
        }
    } else {
        // ObjectStart already written, this shouldn't happen in object context
        // But if it does, write key before scope end
        luisa::string_view key(name);
        write_type(BinType::String);
        write_uint64(key.size());
        write_bytes(key.data(), key.size());
    }

    // Write scope end
    write_type(BinType::ScopeEnd);
}

void BinWriter::add(bool bool_value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::Bool);
    uint8_t val = bool_value ? 1 : 0;
    write_bytes(&val, sizeof(uint8_t));
}

void BinWriter::add(int64_t int_value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::Int64);
    write_bytes(&int_value, sizeof(int64_t));
}

void BinWriter::add(uint64_t uint_value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::UInt64);
    write_bytes(&uint_value, sizeof(uint64_t));
}

void BinWriter::add(double float_value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::Double);
    write_bytes(&float_value, sizeof(double));
}

void BinWriter::add(luisa::string_view str) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::String);
    write_uint64(str.size());
    write_bytes(str.data(), str.size());
}

void BinWriter::add_arr(luisa::span<int64_t const> int_values) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::ArrayStart);
    write_uint64(int_values.size());
    write_bytes(int_values.data(), int_values.size() * sizeof(int64_t));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<uint64_t const> uint_values) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::ArrayStart);
    write_uint64(uint_values.size());
    write_bytes(uint_values.data(), uint_values.size() * sizeof(uint64_t));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<double const> double_values) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::ArrayStart);
    write_uint64(double_values.size());
    write_bytes(double_values.data(), double_values.size() * sizeof(double));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<bool const> bool_values) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);
    scope.array_size++;

    write_type(BinType::ArrayStart);
    write_uint64(bool_values.size());
    for (auto val : bool_values) {
        uint8_t b = val ? 1 : 0;
        write_bytes(&b, sizeof(uint8_t));
    }
    write_type(BinType::ScopeEnd);
}

void BinWriter::add(bool bool_value, char const *name) {
    if (!name) {
        add(bool_value);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write value
    write_type(BinType::Bool);
    uint8_t val = bool_value ? 1 : 0;
    write_bytes(&val, sizeof(uint8_t));
}

void BinWriter::add(int64_t int_value, char const *name) {
    if (!name) {
        add(int_value);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write value
    write_type(BinType::Int64);
    write_bytes(&int_value, sizeof(int64_t));
}

void BinWriter::add(uint64_t uint_value, char const *name) {
    if (!name) {
        add(uint_value);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write value
    write_type(BinType::UInt64);
    write_bytes(&uint_value, sizeof(uint64_t));
}

void BinWriter::add(double float_value, char const *name) {
    if (!name) {
        add(float_value);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write value
    write_type(BinType::Double);
    write_bytes(&float_value, sizeof(double));
}

void BinWriter::add(luisa::string_view str, char const *name) {
    if (!name) {
        add(str);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write value
    write_type(BinType::String);
    write_uint64(str.size());
    write_bytes(str.data(), str.size());
}

void BinWriter::add_arr(luisa::span<int64_t const> int_values, char const *name) {
    if (!name) {
        add_arr(int_values);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write array
    write_type(BinType::ArrayStart);
    write_uint64(int_values.size());
    write_bytes(int_values.data(), int_values.size() * sizeof(int64_t));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<uint64_t const> uint_values, char const *name) {
    if (!name) {
        add_arr(uint_values);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write array
    write_type(BinType::ArrayStart);
    write_uint64(uint_values.size());
    write_bytes(uint_values.data(), uint_values.size() * sizeof(uint64_t));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<double const> double_values, char const *name) {
    if (!name) {
        add_arr(double_values);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write array
    write_type(BinType::ArrayStart);
    write_uint64(double_values.size());
    write_bytes(double_values.data(), double_values.size() * sizeof(double));
    write_type(BinType::ScopeEnd);
}

void BinWriter::add_arr(luisa::span<bool const> bool_values, char const *name) {
    if (!name) {
        add_arr(bool_values);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write array
    write_type(BinType::ArrayStart);
    write_uint64(bool_values.size());
    for (auto val : bool_values) {
        uint8_t b = val ? 1 : 0;
        write_bytes(&b, sizeof(uint8_t));
    }
    write_type(BinType::ScopeEnd);
}

void BinWriter::bytes(luisa::span<std::byte const> data) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    
    // For structured access (in array context), write as array element
    if (scope.is_array) {
        scope.array_size++;
        write_type(BinType::Bytes);
        write_uint64(data.size());
        write_bytes(data.data(), data.size());
    } else {
        // For streamed access in object context, we need to write as a named bytes field
        // However, OzzStream doesn't provide a name, so we use a default name
        // This maintains object structure integrity
        luisa::string_view key("__stream_data__");
        write_type(BinType::String);
        write_uint64(key.size());
        write_bytes(key.data(), key.size());
        
        write_type(BinType::Bytes);
        write_uint64(data.size());
        write_bytes(data.data(), data.size());
    }
}

void BinWriter::bytes(luisa::span<std::byte const> data, char const *name) {
    if (!name) {
        bytes(data);
        return;
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Write key
    luisa::string_view key(name);
    write_type(BinType::String);
    write_uint64(key.size());
    write_bytes(key.data(), key.size());

    // Write bytes
    write_type(BinType::Bytes);
    write_uint64(data.size());
    write_bytes(data.data(), data.size());
}

void BinWriter::bytes(void *data, uint64_t size) {
    bytes(luisa::span<std::byte const>{reinterpret_cast<std::byte const *>(data), size});
}

void BinWriter::bytes(void *data, uint64_t size, char const *name) {
    bytes(luisa::span<std::byte const>{reinterpret_cast<std::byte const *>(data), size}, name);
}

luisa::BinaryBlob BinWriter::write_to() const {
    // Update array sizes in buffer before creating the blob
    // We need to modify the buffer, so create a mutable copy
    luisa::vector<std::byte> buffer_copy = buffer_;

    // Update all array sizes that were written as placeholders
    for (const auto &scope : _scope) {
        if (scope.is_array && scope.array_size_pos > 0) {
            // Update the array size at the saved position
            std::memcpy(buffer_copy.data() + scope.array_size_pos, &scope.array_size, sizeof(uint64_t));
        }
    }

    // Create a copy of the updated buffer
    auto size = buffer_copy.size();
    auto data = vengine_malloc(size);
    std::memcpy(data, buffer_copy.data(), size);
    return luisa::BinaryBlob{
        reinterpret_cast<std::byte *>(data),
        size,
        [](void *ptr) { if(ptr) vengine_free(ptr); }};
}

bool BinWriter::is_current_scope_array() const {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    return _scope.back().is_array;
}

void BinWriter::clear_alloc() {
    buffer_.clear();
    _scope.clear();
}

void BinWriter::reset() {
    pos_ = 0;
}

void BinWriter::reset_buffer() {
    buffer_.clear();
    _scope.clear();
}

bool BinWriter::move_next(uint64_t size, uint64_t &old_pos) {
    if (pos_ + size > buffer_.size()) [[unlikely]] {
        LUISA_ERROR("[BinWriter] out of range {} + {} > {}", pos_, size, buffer_.size());
        return false;
    } else {
        old_pos = pos_;
        pos_ += size;
        return true;
    }
}

// BinReader implementation
BinReader::BinReader(luisa::span<std::byte const> data) : pos_(0), valid_(true) {
    buffer_.resize(data.size());
    std::memcpy(buffer_.data(), data.data(), data.size());

    // Read root type
    BinType root_type;
    if (!read_type(root_type)) {
        valid_ = false;
        return;
    }

    if (root_type == BinType::ArrayStart) {
        BinScope scope;
        scope.is_array = true;
        if (!read_uint64(scope.array_size)) {
            valid_ = false;
            return;
        }
        scope.array_index = 0;
        scope.in_object = false;
        _scope.emplace_back(scope);
    } else if (root_type == BinType::ObjectStart) {
        BinScope scope;
        scope.is_array = false;
        scope.in_object = true;
        _scope.emplace_back(scope);
    } else {
        valid_ = false;
    }
}

BinReader::BinReader(luisa::BinaryBlob const &blob) : BinReader(luisa::span<std::byte const>{blob.data(), blob.size()}) {}

BinReader::~BinReader() {
    LUISA_ASSERT(_scope.size() == 1);
}

bool BinReader::read_type(BinType &type) {
    if (pos_ + sizeof(BinType) > buffer_.size()) {
        return false;
    }
    std::memcpy(&type, buffer_.data() + pos_, sizeof(BinType));
    pos_ += sizeof(BinType);
    return true;
}

bool BinReader::read_uint64(uint64_t &value) {
    if (pos_ + sizeof(uint64_t) > buffer_.size()) {
        return false;
    }
    std::memcpy(&value, buffer_.data() + pos_, sizeof(uint64_t));
    pos_ += sizeof(uint64_t);
    return true;
}

bool BinReader::read_string(luisa::string &value) {
    uint64_t size;
    if (!read_uint64(size)) {
        return false;
    }
    if (pos_ + size > buffer_.size()) {
        return false;
    }
    value.resize(size);
    std::memcpy(value.data(), buffer_.data() + pos_, size);
    pos_ += size;
    return true;
}

bool BinReader::read_bytes_internal(void *data, uint64_t size) {
    if (pos_ + size > buffer_.size()) {
        return false;
    }
    std::memcpy(data, buffer_.data() + pos_, size);
    pos_ += size;
    return true;
}

bool BinReader::check_type(BinType expected) {
    BinType type;
    if (!read_type(type)) {
        return false;
    }
    return type == expected;
}

bool BinReader::start_array(uint64_t &size) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type)) {
        return false;
    }

    if (type != BinType::ArrayStart) {
        // Rewind
        pos_ -= sizeof(BinType);
        return false;
    }

    uint64_t arr_size;
    if (!read_uint64(arr_size)) {
        return false;
    }

    BinScope new_scope;
    new_scope.is_array = true;
    new_scope.array_size = arr_size;
    new_scope.array_index = 0;
    new_scope.in_object = false;
    _scope.emplace_back(new_scope);

    scope.array_index++;
    return true;
}

bool BinReader::start_object() {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type)) {
        return false;
    }

    if (type != BinType::ObjectStart) {
        // Rewind
        pos_ -= sizeof(BinType);
        return false;
    }

    BinScope new_scope;
    new_scope.is_array = false;
    new_scope.in_object = true;
    _scope.emplace_back(new_scope);

    scope.array_index++;
    return true;
}

bool BinReader::start_array(uint64_t &size, char const *name) {
    if (!name) {
        return start_array(size);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read array start
            BinType arr_type;
            if (!read_type(arr_type) || arr_type != BinType::ArrayStart) {
                return false;
            }

            if (!read_uint64(size)) {
                return false;
            }

            BinScope new_scope;
            new_scope.is_array = true;
            new_scope.array_size = size;
            new_scope.array_index = 0;
            new_scope.in_object = false;
            _scope.emplace_back(new_scope);

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::start_object(char const *name) {
    if (!name) {
        return start_object();
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read object start
            BinType obj_type;
            if (!read_type(obj_type) || obj_type != BinType::ObjectStart) {
                return false;
            }

            BinScope new_scope;
            new_scope.is_array = false;
            new_scope.in_object = true;
            _scope.emplace_back(new_scope);

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

void BinReader::end_scope() {
    LUISA_ASSERT(!_scope.empty());

    // Read scope end marker
    BinType type;
    if (!read_type(type) || type != BinType::ScopeEnd) {
        LUISA_ERROR("[BinReader] Expected scope end marker");
    }

    _scope.pop_back();
}

bool BinReader::read(bool &value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::Bool) {
        return false;
    }

    uint8_t val;
    if (!read_bytes_internal(&val, sizeof(uint8_t))) {
        return false;
    }

    value = val != 0;
    scope.array_index++;
    return true;
}

bool BinReader::read(int64_t &value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::Int64) {
        return false;
    }

    if (!read_bytes_internal(&value, sizeof(int64_t))) {
        return false;
    }

    scope.array_index++;
    return true;
}

bool BinReader::read(uint64_t &value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::UInt64) {
        return false;
    }

    if (!read_bytes_internal(&value, sizeof(uint64_t))) {
        return false;
    }

    scope.array_index++;
    return true;
}

bool BinReader::read(double &value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::Double) {
        return false;
    }

    if (!read_bytes_internal(&value, sizeof(double))) {
        return false;
    }

    scope.array_index++;
    return true;
}

bool BinReader::read(luisa::string &value) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::String) {
        return false;
    }

    if (!read_string(value)) {
        return false;
    }

    scope.array_index++;
    return true;
}

bool BinReader::read(bool &value, char const *name) {
    if (!name) {
        return read(value);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            // Rewind to before reading ScopeEnd
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read value
            BinType val_type;
            if (!read_type(val_type) || val_type != BinType::Bool) {
                return false;
            }

            uint8_t val;
            if (!read_bytes_internal(&val, sizeof(uint8_t))) {
                return false;
            }

            value = val != 0;
            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::read(int64_t &value, char const *name) {
    if (!name) {
        return read(value);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read value
            BinType val_type;
            if (!read_type(val_type) || val_type != BinType::Int64) {
                return false;
            }

            if (!read_bytes_internal(&value, sizeof(int64_t))) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::read(uint64_t &value, char const *name) {
    if (!name) {
        return read(value);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read value
            BinType val_type;
            if (!read_type(val_type) || val_type != BinType::UInt64) {
                return false;
            }

            if (!read_bytes_internal(&value, sizeof(uint64_t))) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::read(double &value, char const *name) {
    if (!name) {
        return read(value);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read value
            BinType val_type;
            if (!read_type(val_type) || val_type != BinType::Double) {
                return false;
            }

            if (!read_bytes_internal(&value, sizeof(double))) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::read(luisa::string &value, char const *name) {
    if (!name) {
        return read(value);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read value
            BinType val_type;
            if (!read_type(val_type) || val_type != BinType::String) {
                return false;
            }

            if (!read_string(value)) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::bytes(luisa::vector<std::byte> &data) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(scope.is_array);

    if (scope.array_index >= scope.array_size) {
        return false;
    }

    BinType type;
    if (!read_type(type) || type != BinType::Bytes) {
        return false;
    }

    uint64_t size;
    if (!read_uint64(size)) {
        return false;
    }

    data.resize(size);
    if (!read_bytes_internal(data.data(), size)) {
        return false;
    }

    scope.array_index++;
    return true;
}

bool BinReader::bytes(luisa::vector<std::byte> &data, char const *name) {
    if (!name) {
        return bytes(data);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read bytes
            BinType bytes_type;
            if (!read_type(bytes_type) || bytes_type != BinType::Bytes) {
                return false;
            }

            uint64_t size;
            if (!read_uint64(size)) {
                return false;
            }

            data.resize(size);
            if (!read_bytes_internal(data.data(), size)) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

bool BinReader::bytes(void *data, uint64_t size) {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    
    // For structured access (in array context), read as array element with type marker
    if (scope.is_array) {
        if (scope.array_index >= scope.array_size) {
            return false;
        }

        BinType type;
        if (!read_type(type) || type != BinType::Bytes) {
            return false;
        }

        uint64_t bytes_size;
        if (!read_uint64(bytes_size)) {
            return false;
        }

        // Check if provided buffer is large enough
        if (bytes_size > size) {
            return false;
        }

        if (!read_bytes_internal(data, bytes_size)) {
            return false;
        }

        scope.array_index++;
        return true;
    } else {
        // For streamed access in object context, read the named bytes field
        // We need to find the "__stream_data__" field and read its value
        luisa::string_view key("__stream_data__");
        
        // Scan through key-value pairs until we find the matching key
        while (true) {
            BinType key_type;
            uint64_t saved_pos = pos_;
            if (!read_type(key_type)) {
                return false;
            }
            
            // Check if we've reached the end of the object
            if (key_type == BinType::ScopeEnd) {
                pos_ = saved_pos;
                return false;
            }
            
            if (key_type != BinType::String) {
                return false;
            }
            
            luisa::string read_key;
            if (!read_string(read_key)) {
                return false;
            }
            
            if (read_key == key) {
                // Found matching key, read bytes value
                BinType bytes_type;
                if (!read_type(bytes_type) || bytes_type != BinType::Bytes) {
                    return false;
                }
                
                uint64_t bytes_size;
                if (!read_uint64(bytes_size)) {
                    return false;
                }
                
                // Check if provided buffer is large enough
                if (bytes_size > size) {
                    return false;
                }
                
                if (!read_bytes_internal(data, bytes_size)) {
                    return false;
                }
                
                scope.last_key = read_key;
                return true;
            } else {
                // Key doesn't match, skip value
                if (!skip_value()) {
                    return false;
                }
            }
        }
    }
}

bool BinReader::bytes(void *data, uint64_t size, char const *name) {
    if (!name) {
        return bytes(data, size);
    }

    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    LUISA_DEBUG_ASSERT(!scope.is_array && scope.in_object);

    // Scan through key-value pairs until we find the matching key
    while (true) {
        BinType key_type;
        uint64_t saved_pos = pos_;
        if (!read_type(key_type)) {
            return false;
        }

        // Check if we've reached the end of the object
        if (key_type == BinType::ScopeEnd) {
            pos_ = saved_pos;
            return false;
        }

        if (key_type != BinType::String) {
            return false;
        }

        luisa::string key;
        if (!read_string(key)) {
            return false;
        }

        if (key == name) {
            // Found matching key, read bytes
            BinType bytes_type;
            if (!read_type(bytes_type) || bytes_type != BinType::Bytes) {
                return false;
            }

            uint64_t bytes_size;
            if (!read_uint64(bytes_size)) {
                return false;
            }

            // Check if provided buffer is large enough
            if (bytes_size > size) {
                return false;
            }

            if (!read_bytes_internal(data, bytes_size)) {
                return false;
            }

            scope.last_key = key;
            return true;
        } else {
            // Key doesn't match, skip value
            if (!skip_value()) {
                return false;
            }
        }
    }
}

uint64_t BinReader::last_array_size() const {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    if (!scope.is_array) return 0;
    return scope.array_size;
}

luisa::string_view BinReader::last_key() const {
    LUISA_DEBUG_ASSERT(!_scope.empty());
    auto &scope = _scope.back();
    if (scope.is_array || scope.last_key.empty()) return "";
    return scope.last_key;
}

bool BinReader::skip_value() {
    BinType type;
    if (!read_type(type)) {
        return false;
    }

    switch (type) {
        case BinType::Bool: {
            uint8_t val;
            return read_bytes_internal(&val, sizeof(uint8_t));
        }
        case BinType::Int64: {
            int64_t val;
            return read_bytes_internal(&val, sizeof(int64_t));
        }
        case BinType::UInt64: {
            uint64_t val;
            return read_bytes_internal(&val, sizeof(uint64_t));
        }
        case BinType::Double: {
            double val;
            return read_bytes_internal(&val, sizeof(double));
        }
        case BinType::String: {
            uint64_t size;
            if (!read_uint64(size)) return false;
            pos_ += size;// Skip string data
            return true;
        }
        case BinType::Bytes: {
            uint64_t size;
            if (!read_uint64(size)) return false;
            pos_ += size;// Skip bytes data
            return true;
        }
        case BinType::ArrayStart: {
            uint64_t size;
            if (!read_uint64(size)) return false;
            // Skip all array elements
            for (uint64_t i = 0; i < size; ++i) {
                if (!skip_value()) return false;
            }
            // Read scope end
            BinType end_type;
            if (!read_type(end_type) || end_type != BinType::ScopeEnd) {
                return false;
            }
            return true;
        }
        case BinType::ObjectStart: {
            // Skip all object key-value pairs until scope end
            // Object format: key1 (String) + value1 + key2 (String) + value2 + ... + ScopeEnd
            while (true) {
                BinType next_type;
                if (!read_type(next_type)) return false;

                if (next_type == BinType::ScopeEnd) {
                    return true;
                }

                // It's a key-value pair: key (string) + value
                if (next_type == BinType::String) {
                    // Skip key
                    uint64_t key_size;
                    if (!read_uint64(key_size)) return false;
                    pos_ += key_size;// Skip key string data
                    // Skip value (recursive call)
                    if (!skip_value()) return false;
                } else {
                    // Unexpected type - should be String (key) or ScopeEnd
                    return false;
                }
            }
        }
        case BinType::ScopeEnd:
            // Should not happen here
            return false;
    }
    return false;
}

bool BinReader::valid() const {
    return valid_;
}

}// namespace rbc
