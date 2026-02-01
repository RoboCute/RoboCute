#pragma once
#include <luisa/core/binary_io.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/vector.h>
#include <luisa/vstl/meta_lib.h>
#include <rbc_config.h>
#include <rbc_core/base.h>

namespace rbc {

enum class BinType : uint8_t {
    Bool = 0,
    Int64 = 1,
    UInt64 = 2,
    Double = 3,
    String = 4,
    ArrayStart = 5,
    ObjectStart = 6,
    ScopeEnd = 7,
    Bytes = 8
};

struct BinScope {
    bool is_array;
    uint64_t array_size;
    uint64_t array_index;
    luisa::string last_key;
    bool in_object;
    bool object_start_written;// For objects in object context, track if ObjectStart has been written
    uint64_t object_start_pos;// Position where ObjectStart should be written (if delayed)
    uint64_t array_size_pos;  // Position where array size was written (for root array or nested arrays)
};

struct RBC_CORE_API BinWriter {
    luisa::vector<std::byte> buffer_;
    luisa::vector<BinScope> _scope;
    uint64_t pos_;
    BinWriter(BinWriter const &) = delete;
    BinWriter(BinWriter &&) = delete;

    BinWriter(bool root_array = false);
    ~BinWriter();

    void start_array();
    void start_object();
    void add_last_scope_to_object();
    void add_last_scope_to_object(char const *name);

protected:
    void clear_alloc();
    void write_bytes(void const *data, uint64_t size);
    void write_type(BinType type);
    void write_uint64(uint64_t value);

public:
    // primitive
    void add(bool bool_value);
    void add(int64_t int_value);
    void add(uint64_t uint_value);
    void add(double float_value);
    void add(luisa::string_view str);

    // batch add for fast
    void add_arr(luisa::span<int64_t const> int_values);
    void add_arr(luisa::span<uint64_t const> uint_values);
    void add_arr(luisa::span<double const> double_values);
    void add_arr(luisa::span<bool const> bool_values);

    // primitive with key
    void add(bool bool_value, char const *name);
    void add(int64_t int_value, char const *name);
    void add(uint64_t uint_value, char const *name);
    void add(double float_value, char const *name);
    void add(luisa::string_view str, char const *name);

    // batch-add with key
    void add_arr(luisa::span<int64_t const> int_values, char const *name);
    void add_arr(luisa::span<uint64_t const> uint_values, char const *name);
    void add_arr(luisa::span<double const> double_values, char const *name);
    void add_arr(luisa::span<bool const> bool_values, char const *name);

    // bytes interface
    void bytes(luisa::span<std::byte const> data);
    void bytes(luisa::span<std::byte const> data, char const *name);
    // Direct bytes interface (void* version)
    void bytes(void *data, uint64_t size);
    void bytes(void *data, uint64_t size, char const *name);

public:
    [[nodiscard]] luisa::BinaryBlob write_to() const;
    [[nodiscard]] bool is_current_scope_array() const;
    luisa::span<std::byte> buffer() {
        return buffer_;
    }
    void reset();
    void reset_buffer();
    bool move_next(uint64_t size, uint64_t &old_pos);
};

struct RBC_CORE_API BinReader {
    luisa::vector<std::byte> buffer_;
    uint64_t pos_;
    luisa::vector<BinScope> _scope;
    bool valid_;

    explicit BinReader(luisa::span<std::byte const> data);
    BinReader(luisa::BinaryBlob const &blob);
    ~BinReader();

    [[nodiscard]] uint64_t last_array_size() const;
    [[nodiscard]] luisa::string_view last_key() const;
    bool start_array(uint64_t &size);
    bool start_object();
    bool start_array(uint64_t &size, char const *name);
    bool start_object(char const *name);
    void end_scope();

    // primitive
    bool read(bool &value);
    bool read(int64_t &value);
    bool read(uint64_t &value);
    bool read(double &value);
    bool read(luisa::string &value);
    bool read(BasicDeserDataType &value);

    // primitive with name
    bool read(bool &value, char const *name);
    bool read(int64_t &value, char const *name);
    bool read(uint64_t &value, char const *name);
    bool read(double &value, char const *name);
    bool read(luisa::string &value, char const *name);
    bool read(BasicDeserDataType &value, char const *name);

    // bytes interface
    bool bytes(luisa::vector<std::byte> &data);
    bool bytes(luisa::vector<std::byte> &data, char const *name);
    // Direct bytes interface (void* version)
    bool bytes(void *data, uint64_t size);
    bool bytes(void *data, uint64_t size, char const *name);

    bool valid() const;

protected:
    bool read_bytes_internal(void *data, uint64_t size);
    bool read_type(BinType &type);
    bool read_uint64(uint64_t &value);
    bool read_string(luisa::string &value);
    bool check_type(BinType expected);
    bool skip_value();// Skip current value (used when key doesn't match in object)
};

}// namespace rbc