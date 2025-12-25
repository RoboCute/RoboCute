#pragma once
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl/type_traits.h>
#include <luisa/core/stl/unordered_map.h>
#include <luisa/vstl/stack_allocator.h>
#include <luisa/core/binary_io.h>
#include <luisa/vstl/hash_map.h>
#include <luisa/vstl/v_guid.h>
#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>

#include <rbc_core/base.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/json_serde.h>
namespace rbc {
// Forward declaration for Serialize template
template<typename T>
struct Serialize;
namespace concepts {
template<typename T, size_t N = 0u>
struct is_matrix_impl : std::false_type {};

template<size_t N>
struct is_matrix_impl<luisa::Matrix<float, N>, N> : std::true_type {};

template<size_t N>
struct is_matrix_impl<luisa::Matrix<float, N>, 0u> : std::true_type {};

template<size_t N>
struct is_matrix_impl<luisa::Matrix<double, N>, N> : std::true_type {};

template<size_t N>
struct is_matrix_impl<luisa::Matrix<double, N>, 0u> : std::true_type {};

template<typename T, size_t N = 0u>
using is_matrix = is_matrix_impl<std::remove_cvref_t<T>, N>;
template<typename T, size_t N = 0u>
constexpr auto is_matrix_v = is_matrix<T, N>::value;

template<typename T>
concept SerWriter = requires(T t) {
    t.start_array();
    t.start_object();
    t.add_last_scope_to_object();
    t.add_last_scope_to_object(std::declval<char const *>());
    // array
    t.add(std::declval<bool>());
    t.add(std::declval<int64_t>());
    t.add(std::declval<uint64_t>());
    t.add(std::declval<double>());
    t.add_arr(std::declval<luisa::span<int64_t const>>());
    t.add_arr(std::declval<luisa::span<uint64_t const>>());
    t.add_arr(std::declval<luisa::span<double const>>());
    t.add_arr(std::declval<luisa::span<bool const>>());
    t.add(std::declval<luisa::string_view>());
    // object
    t.add(std::declval<bool>(), std::declval<char const *>());
    t.add(std::declval<int64_t>(), std::declval<char const *>());
    t.add(std::declval<uint64_t>(), std::declval<char const *>());
    t.add(std::declval<double>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<int64_t const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<uint64_t const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<double const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<bool const>>(), std::declval<char const *>());
    t.add(std::declval<luisa::string_view>(), std::declval<char const *>());

    std::same_as<decltype(t.write_to()), luisa::BinaryBlob>;
};
template<typename T>
concept SerReader = requires(T t) {
    std::same_as<decltype(t.last_key()), luisa::string_view>;
    std::same_as<bool, decltype(t.start_array(lvalue_declval<uint64_t>()))>;
    std::same_as<bool, decltype(t.start_object())>;
    // array
    std::same_as<bool, decltype(t.read(lvalue_declval<bool>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<int64_t>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<uint64_t>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<double>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<luisa::string>()))>;
    // object
    std::same_as<bool, decltype(t.read(lvalue_declval<bool>(), std::declval<char const *>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<int64_t>(), std::declval<char const *>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<uint64_t>(), std::declval<char const *>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<double>(), std::declval<char const *>()))>;
    std::same_as<bool, decltype(t.read(lvalue_declval<luisa::string>(), std::declval<char const *>()))>;
    t.end_scope();
};

// Concepts for detecting Serialize<T> specializations
template<typename T, typename ArchiveWriter>
concept HasSerializeWrite = requires(ArchiveWriter &w, const T &v) {
    { Serialize<T>::write(w, v) } -> std::same_as<void>;
};

template<typename T, typename ArchiveReader>
concept HasSerializeRead = requires(ArchiveReader &r, T &v) {
    { Serialize<T>::read(r, v) } -> std::same_as<bool>;
};

}// namespace concepts

// Forward declarations
template<concepts::SerWriter Base>
struct Serializer;
template<concepts::SerReader Base>
struct DeSerializer;

// Helper traits to extract underlying Writer/Reader types
namespace detail {
template<typename T>
struct extract_writer_type {
    using type = T;
};
template<concepts::SerWriter Base>
struct extract_writer_type<Serializer<Base>> {
    using type = Base;
};
template<typename T>
using extract_writer_type_t = typename extract_writer_type<T>::type;

template<typename T>
struct extract_reader_type {
    using type = T;
};
template<concepts::SerReader Base>
struct extract_reader_type<DeSerializer<Base>> {
    using type = Base;
};
template<typename T>
using extract_reader_type_t = typename extract_reader_type<T>::type;

}// namespace detail

// Archive adapters for Serialize<T> pattern
namespace detail {
// ArchiveWriteAdapter wraps a Serializer to provide value<T>() interface compatible with Serialize<T>
template<concepts::SerWriter Writer>
struct ArchiveWriteAdapter {
    Serializer<Writer> &serializer;
    Writer &writer;
    // Stack to track sequential index counters for each object scope
    // Each entry is a pair: (is_object_scope, counter)
    luisa::vector<std::pair<bool, uint64_t>> _scope_counters;

    explicit ArchiveWriteAdapter(Serializer<Writer> &s) : serializer(s), writer(static_cast<Writer &>(s)) {}

    // Helper to generate sequential key from counter
    // Note: JsonWriter::add() methods now copy the string, so we can use a temporary string
    luisa::string _generate_sequential_key(uint64_t index) {
        // Convert index to string using std::to_string, then convert to luisa::string
        auto str = std::to_string(index);
        return luisa::string(str);
    }

    // Helper to check if current scope is an object and get/advance counter
    bool _is_current_scope_object() {
        // Try to use JsonWriter's method if available
        if constexpr (requires { writer.is_current_scope_array(); }) {
            return !writer.is_current_scope_array();
        }
        // Fallback: check if we have a counter on the stack
        return !_scope_counters.empty() && _scope_counters.back().first;
    }

    // Forward all writer methods
    void start_array() {
        writer.start_array();
        // Push a marker for array scope (not an object, so no counter needed)
        _scope_counters.emplace_back(false, 0);
    }
    void start_object() {
        writer.start_object();
        // Push a new counter for object scope
        _scope_counters.emplace_back(true, 0);
    }
    void add_last_scope_to_object() {
        writer.add_last_scope_to_object();
        // Pop the scope counter when exiting a scope
        if (!_scope_counters.empty()) {
            _scope_counters.pop_back();
        }
    }
    void add_last_scope_to_object(char const *name) {
        writer.add_last_scope_to_object(name);
        // Pop the scope counter when exiting a scope
        if (!_scope_counters.empty()) {
            _scope_counters.pop_back();
        }
    }

    // value<T>() method that uses Serialize<T> specialization
    template<typename T>
    void value(const T &v) {
        // Check if we're in an object context and should use sequential keys
        bool use_sequential_key = _is_current_scope_object();
        char const *key_name = nullptr;

        luisa::string sequential_key_str;
        if (use_sequential_key && !_scope_counters.empty()) {
            // Generate sequential key from counter
            uint64_t index = _scope_counters.back().second;
            sequential_key_str = _generate_sequential_key(index);
            key_name = sequential_key_str.c_str();
            // Advance counter for next value
            _scope_counters.back().second++;
        }

        if constexpr (requires { Serialize<T>::write(*this, v); }) {
            if (use_sequential_key && key_name) {
                // Custom type with Serialize<T> specialization in object context
                writer.start_object();
                Serialize<T>::write(*this, v);
                writer.add_last_scope_to_object(key_name);
            } else {
                // In array context or no sequential key needed
                Serialize<T>::write(*this, v);
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            if (use_sequential_key && key_name) {
                writer.add(v, key_name);
            } else {
                writer.add(v);
            }
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                if (use_sequential_key && key_name) {
                    writer.add((uint64_t)v, key_name);
                } else {
                    writer.add((uint64_t)v);
                }
            } else {
                if (use_sequential_key && key_name) {
                    writer.add((int64_t)v, key_name);
                } else {
                    writer.add((int64_t)v);
                }
            }
        } else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            if (use_sequential_key && key_name) {
                writer.add((double)v, key_name);
            } else {
                writer.add((double)v);
            }
        } else if constexpr (luisa::is_constructible_v<luisa::string_view, T>) {
            if (use_sequential_key && key_name) {
                writer.add(luisa::string_view{v}, key_name);
            } else {
                writer.add(luisa::string_view{v});
            }
        } else {
            // For other types without Serialize<T>, fall back to serializer's _store method
            if (use_sequential_key && key_name) {
                serializer._store(v, key_name);
            } else {
                serializer._store(v);
            }
        }
    }

    // value<T>(name) method for named values
    template<typename T>
    void value(const T &v, char const *name) {
        if constexpr (requires { Serialize<T>::write(*this, v); }) {
            // Custom type with Serialize<T> specialization
            writer.start_object();
            Serialize<T>::write(*this, v);
            writer.add_last_scope_to_object(name);
        } else if constexpr (std::is_same_v<T, bool>) {
            writer.add(v, name);
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                writer.add((uint64_t)v, name);
            } else {
                writer.add((int64_t)v, name);
            }
        } else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            writer.add((double)v, name);
        } else if constexpr (luisa::is_constructible_v<luisa::string_view, T>) {
            writer.add(luisa::string_view{v}, name);
        } else {
            // For other types without Serialize<T>, fall back to serializer's _store method
            serializer._store(v, name);
        }
    }

    // Direct access to underlying writer for primitive types
    void add(bool bool_value) { writer.add(bool_value); }
    void add(int64_t int_value) { writer.add(int_value); }
    void add(uint64_t uint_value) { writer.add(uint_value); }
    void add(double float_value) { writer.add(float_value); }
    void add(luisa::string_view str) { writer.add(str); }
    void add(bool bool_value, char const *name) { writer.add(bool_value, name); }
    void add(int64_t int_value, char const *name) { writer.add(int_value, name); }
    void add(uint64_t uint_value, char const *name) { writer.add(uint_value, name); }
    void add(double float_value, char const *name) { writer.add(float_value, name); }
    void add(luisa::string_view str, char const *name) { writer.add(str, name); }
    void add_arr(luisa::span<int64_t const> int_values) { writer.add_arr(int_values); }
    void add_arr(luisa::span<uint64_t const> uint_values) { writer.add_arr(uint_values); }
    void add_arr(luisa::span<double const> double_values) { writer.add_arr(double_values); }
    void add_arr(luisa::span<bool const> bool_values) { writer.add_arr(bool_values); }
    void add_arr(luisa::span<int64_t const> int_values, char const *name) { writer.add_arr(int_values, name); }
    void add_arr(luisa::span<uint64_t const> uint_values, char const *name) { writer.add_arr(uint_values, name); }
    void add_arr(luisa::span<double const> double_values, char const *name) { writer.add_arr(double_values, name); }
    void add_arr(luisa::span<bool const> bool_values, char const *name) { writer.add_arr(bool_values, name); }
};

// ArchiveReadAdapter wraps a DeSerializer to provide value<T>() interface compatible with Serialize<T>
template<concepts::SerReader Reader>
struct ArchiveReadAdapter {
    DeSerializer<Reader> &deserializer;
    Reader &reader;
    // Stack to track sequential index counters for each object scope
    // Each entry is a pair: (is_object_scope, counter)
    luisa::vector<std::pair<bool, uint64_t>> _scope_counters;

    explicit ArchiveReadAdapter(DeSerializer<Reader> &d) : deserializer(d), reader(static_cast<Reader &>(d)) {}

    // Helper to generate sequential key from counter
    luisa::string _generate_sequential_key(uint64_t index) {
        // Convert index to string (simple implementation, can be optimized)
        char buffer[32];
        auto len = snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(index));
        return luisa::string(buffer, len);
    }

    // Helper to check if current scope is an object
    bool _is_current_scope_object() {
        // Check if we have a counter on the stack
        return !_scope_counters.empty() && _scope_counters.back().first;
    }

    // Forward all reader methods
    bool start_array(uint64_t &size) {
        bool result = reader.start_array(size);
        if (result) {
            // Push a marker for array scope (not an object, so no counter needed)
            _scope_counters.emplace_back(false, 0);
        }
        return result;
    }
    bool start_array(uint64_t &size, char const *name) {
        bool result = reader.start_array(size, name);
        if (result) {
            // Push a marker for array scope (not an object, so no counter needed)
            _scope_counters.emplace_back(false, 0);
        }
        return result;
    }
    bool start_object() {
        bool result = reader.start_object();
        if (result) {
            // Push a new counter for object scope
            _scope_counters.emplace_back(true, 0);
        }
        return result;
    }
    bool start_object(char const *name) {
        bool result = reader.start_object(name);
        if (result) {
            // Push a new counter for object scope
            _scope_counters.emplace_back(true, 0);
        }
        return result;
    }
    void end_scope() {
        reader.end_scope();
        // Pop the scope counter when exiting a scope
        if (!_scope_counters.empty()) {
            _scope_counters.pop_back();
        }
    }

    // value<T>() method that uses Serialize<T> specialization
    template<typename T>
    bool value(T &v) {
        // Check if we're in an object context and should use sequential keys
        bool use_sequential_key = _is_current_scope_object();
        char const *key_name = nullptr;
        luisa::string sequential_key;

        if (use_sequential_key && !_scope_counters.empty()) {
            // Generate sequential key from counter
            uint64_t index = _scope_counters.back().second;
            sequential_key = _generate_sequential_key(index);
            key_name = sequential_key.c_str();
            // Advance counter for next value
            _scope_counters.back().second++;
        }

        if constexpr (requires { Serialize<T>::read(*this, v); }) {
            // Serialize<T>::read expects to be called in an object context
            // If we're in an object context with sequential keys, we need to enter the nested object first
            if (use_sequential_key && key_name) {
                // We're in an object context, so we need to read a nested object with the sequential key
                if (!reader.start_object(key_name)) return false;
                bool result = Serialize<T>::read(*this, v);
                reader.end_scope();
                return result;
            } else {
                // In array context, we need to enter the object first
                if (!reader.start_object()) return false;
                bool result = Serialize<T>::read(*this, v);
                reader.end_scope();
                return result;
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            if (use_sequential_key && key_name) {
                return reader.read(v, key_name);
            } else {
                return reader.read(v);
            }
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                uint64_t temp;
                bool result = use_sequential_key && key_name ? reader.read(temp, key_name) : reader.read(temp);
                if (result) v = (T)temp;
                return result;
            } else {
                int64_t temp;
                bool result = use_sequential_key && key_name ? reader.read(temp, key_name) : reader.read(temp);
                if (result) v = (T)temp;
                return result;
            }
        } else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            double temp;
            bool result = use_sequential_key && key_name ? reader.read(temp, key_name) : reader.read(temp);
            if (result) v = (T)temp;
            return result;
        } else if constexpr (std::is_same_v<T, luisa::string>) {
            if (use_sequential_key && key_name) {
                return reader.read(v, key_name);
            } else {
                return reader.read(v);
            }
        } else {
            // For other types without Serialize<T>, fall back to deserializer's _load method
            if (use_sequential_key && key_name) {
                return deserializer._load(v, key_name);
            } else {
                return deserializer._load(v);
            }
        }
    }

    // value<T>(name) method for named values
    template<typename T>
    bool value(T &v, char const *name) {
        if constexpr (requires { Serialize<T>::read(*this, v); }) {
            // Custom type with Serialize<T> specialization
            if (!reader.start_object(name)) return false;
            bool result = Serialize<T>::read(*this, v);
            reader.end_scope();
            return result;
        } else if constexpr (std::is_same_v<T, bool>) {
            return reader.read(v, name);
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                uint64_t temp;
                bool result = reader.read(temp, name);
                if (result) v = (T)temp;
                return result;
            } else {
                int64_t temp;
                bool result = reader.read(temp, name);
                if (result) v = (T)temp;
                return result;
            }
        } else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            double temp;
            bool result = reader.read(temp, name);
            if (result) v = (T)temp;
            return result;
        } else if constexpr (std::is_same_v<T, luisa::string>) {
            return reader.read(v, name);
        } else {
            // For other types without Serialize<T>, fall back to deserializer's _load method
            return deserializer._load(v, name);
        }
    }

    // Direct access to underlying reader for primitive types
    bool read(bool &value) { return reader.read(value); }
    bool read(int64_t &value) { return reader.read(value); }
    bool read(uint64_t &value) { return reader.read(value); }
    bool read(double &value) { return reader.read(value); }
    bool read(luisa::string &value) { return reader.read(value); }
    bool read(bool &value, char const *name) { return reader.read(value, name); }
    bool read(int64_t &value, char const *name) { return reader.read(value, name); }
    bool read(uint64_t &value, char const *name) { return reader.read(value, name); }
    bool read(double &value, char const *name) { return reader.read(value, name); }
    bool read(luisa::string &value, char const *name) { return reader.read(value, name); }
};

}// namespace detail
namespace detail {
template<typename T>
struct is_unordered_map {
    static constexpr bool value = false;
};
template<typename K, typename V>
struct is_unordered_map<luisa::unordered_map<K, V>> {
    static constexpr bool value = true;
    using KeyType = K;
    using ValueType = V;
};
template<typename K, typename V>
struct is_unordered_map<vstd::HashMap<K, V>> {
    static constexpr bool value = true;
    using KeyType = K;
    using ValueType = V;
};
template<typename K>
struct is_vector {
    static constexpr bool value = false;
};
template<typename Ele>
struct is_vector<luisa::vector<Ele>> {
    static constexpr bool value = true;
};
template<typename T>
static constexpr bool serializable_native_type_v = std::is_integral_v<T> || std::is_floating_point_v<T> || luisa::is_vector_v<T> || concepts::is_matrix_v<T> || rbc::detail::is_unordered_map<T>::value || std::is_same_v<std::decay_t<T>, luisa::string> || std::is_same_v<T, vstd::Guid> || std::is_same_v<std::decay_t<T>, luisa::string_view> || std::is_same_v<T, luisa::half> || std::is_same_v<T, bool> || is_vector<T>::value || std::is_enum_v<T>;

// Helper trait to check if Serialize<T> has write specialization
// This properly handles type aliases by checking the actual type
template<typename T, typename Ser, typename = void>
struct has_serialize_write : std::false_type {};

template<typename T, typename Ser>
struct has_serialize_write<T, Ser, std::void_t<decltype(Serialize<T>::write(std::declval<detail::ArchiveWriteAdapter<detail::extract_writer_type_t<Ser>> &>(), std::declval<T const &>()))>> : std::true_type {};

// Helper trait to check if Serialize<T> has read specialization
template<typename T, typename DeSer, typename = void>
struct has_serialize_read : std::false_type {};

template<typename T, typename DeSer>
struct has_serialize_read<T, DeSer, std::void_t<decltype(Serialize<T>::read(std::declval<detail::ArchiveReadAdapter<detail::extract_reader_type_t<DeSer>> &>(), std::declval<T &>()))>> : std::true_type {};

template<typename T, typename Ser>
static constexpr bool serializable_struct_type_v =
    has_serialize_write<T, Ser>::value ||
    requires { std::declval<T>().rbc_objser(lvalue_declval<Ser>()); } ||
    requires { std::declval<T>().rbc_arrser(lvalue_declval<Ser>()); };

template<typename T, typename DeSer>
static constexpr bool deserializable_struct_type_v =
    has_serialize_read<T, DeSer>::value ||
    requires { std::declval<T>().rbc_objdeser(lvalue_declval<DeSer>()); } ||
    requires { std::declval<T>().rbc_arrdeser(lvalue_declval<DeSer>()); };
}// namespace detail

namespace concepts {
template<typename T, typename Ser>
concept SerializableType = rbc::detail::serializable_native_type_v<T> || rbc::detail::serializable_struct_type_v<T, Ser>;

template<typename T, typename DeSer>
concept DeSerializableType = rbc::detail::serializable_native_type_v<T> || rbc::detail::deserializable_struct_type_v<T, DeSer>;

};// namespace concepts
template<concepts::SerWriter Base>
struct Serializer : public Base {

    template<typename... Args>
    Serializer(Args &&...args)
        : Base(std::forward<Args>(args)...) {
    }

    template<concepts::SerializableType<Serializer> T, typename... Args>
    void _store(T const &t, Args... args) {
        // custom type
        if constexpr (std::is_enum_v<T>) {
            static_assert(rbc_rtti_detail::is_rtti_type<T>::value);
            auto strv = EnumSerializer::template get_enum_value_name<T>(luisa::to_underlying(t));
            Base::add(strv, args...);
        } else if constexpr (requires { Serialize<T>::write(std::declval<detail::ArchiveWriteAdapter<Base> &>(), std::declval<T const &>()); }) {
            // Use Serialize<T> specialization (new system)
            detail::ArchiveWriteAdapter<Base> adapter(*static_cast<Serializer<Base> *>(this));
            if constexpr (sizeof...(args) == 0) {
                // No name argument: Serialize<T>::write expects an object context (it calls value() with names),
                // so we need to create an object scope, write to it, then add it to the current scope (array)
                adapter.start_object();
                Serialize<T>::write(adapter, t);
                adapter.add_last_scope_to_object();
            } else {
                // Has name argument, create object scope and add to parent
                adapter.start_object();
                Serialize<T>::write(adapter, t);
                adapter.add_last_scope_to_object(args...);
            }
        } else if constexpr (requires { t.rbc_objser(*this); }) {
            // Fall back to legacy rbc_objser method
            Base::start_object();
            t.rbc_objser(*this);
            Base::add_last_scope_to_object(args...);
        } else if constexpr (requires { t.rbc_arrser(*this); }) {
            // Fall back to legacy rbc_arrser method
            Base::start_array();
            t.rbc_arrser(*this);
            Base::add_last_scope_to_object(args...);
        }
        // bool
        else if constexpr (std::is_same_v<T, bool>) {
            Base::add(t, args...);
        }
        // integer
        else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                Base::add((uint64_t)t, args...);
            } else {
                Base::add((int64_t)t, args...);
            }
        }
        // float
        else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            Base::add((double)t, args...);
        }
        // vector
        else if constexpr (luisa::is_vector_v<T>) {
            constexpr size_t dim = luisa::vector_dimension_v<T>;
            using EleType = luisa::vector_element_t<T>;
            // float vector
            if constexpr (std::is_floating_point_v<EleType> || std::is_same_v<EleType, luisa::half>) {
                double elems[dim];
                for (size_t i = 0; i < dim; ++i) {
                    elems[i] = (double)t[i];
                }
                Base::add_arr(luisa::span<double const>{elems, dim}, args...);
            }
            // int vector
            else if constexpr (std::is_integral_v<EleType>) {
                if constexpr (std::is_unsigned_v<EleType>) {
                    uint64_t elems[dim];
                    for (size_t i = 0; i < dim; ++i) {
                        elems[i] = (uint64_t)t[i];
                    }
                    Base::add_arr(luisa::span<uint64_t const>{elems, dim}, args...);
                } else {
                    int64_t elems[dim];
                    for (size_t i = 0; i < dim; ++i) {
                        elems[i] = (int64_t)t[i];
                    }
                    Base::add_arr(luisa::span<int64_t const>{elems, dim}, args...);
                }
            }
            // bool vector
            else if constexpr (std::is_same_v<bool, EleType>) {
                bool elems[dim];
                for (size_t i = 0; i < dim; ++i) {
                    elems[i] = t[i];
                }
                Base::add_arr(luisa::span<bool const>{elems, dim}, args...);
            } else {
                static_assert(luisa::always_false_v<T>, "Invalid vector type.");
            }
        } else if constexpr (concepts::is_matrix_v<T>) {
            constexpr size_t dim = luisa::matrix_dimension_v<T>;
            double arr[dim * dim];
            for (size_t y = 0; y < dim; ++y)
                for (size_t x = 0; x < dim; ++x) {
                    arr[x + y * dim] = t.cols[y][x];
                }
            Base::add_arr(luisa::span<double const>{arr, dim * dim}, args...);
        }
        // string
        else if constexpr (luisa::is_constructible_v<luisa::string_view, T>) {
            Base::add(luisa::string_view{t}, args...);
        }
        // kv map
        else if constexpr (rbc::detail::is_unordered_map<T>::value) {
            Base::start_array();
            for (auto &&i : t) {
                if constexpr (requires { i.first.c_str(); }) {
                    this->_store(luisa::string_view{i.first});
                    this->_store(i.second);
                } else {
                    static_assert(luisa::always_false_v<T>, "Invalid HashMap key-value type.");
                }
            }
            Base::add_last_scope_to_object(args...);
        } else if constexpr (rbc::detail::is_vector<T>::value) {
            Base::start_array();
            for (auto &&i : t) {
                this->_store(i);
            }
            Base::add_last_scope_to_object(args...);
        }
        // duck type (this is no good)
        // else if constexpr (requires {t.data(); t. size(); }) {
        //     auto i = t.data();
        //     auto end = i + t.size();
        //     Base::start_array();
        //     while (i != end) {
        //         this->_store(*i);
        //         ++i;
        //     }
        //     Base::add_last_scope_to_object(args...);

        // } else if constexpr (requires { t.begin() ; t.end(); }) {
        //     auto i = t.begin();
        //     auto end = i.end();
        //     Base::start_array();
        //     while (i != end) {
        //         this->_store(*i);
        //         ++i;
        //     }
        //     Base::add_last_scope_to_object(args...);
        // }
        else if constexpr (std::is_same_v<T, vstd::Guid>) {
            auto str = t.to_base64();
            Base::add(luisa::string_view{str}, args...);
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid serialize type.");
        }
    }
    void clear() {
        Base::clear_alloc();
    }
    template<typename T>
    luisa::BinaryBlob serialize(char const *name, T const &t) {
        Base::clear_alloc();
        _store(t, name);
        return Base::write_to();
    }
};

template<concepts::SerReader Base>
struct DeSerializer : public Base {
    template<typename... Args>
    DeSerializer(Args &&...args)
        : Base(std::forward<Args>(args)...) {
    }
    template<concepts::DeSerializableType<DeSerializer> T, typename... Args>
    bool _load(T &t, Args... args) {
        // custom type
        if constexpr (std::is_enum_v<T>) {
            static_assert(rbc_rtti_detail::is_rtti_type<T>::value);
            luisa::string str;
            Base::read(str, args...);
            auto value = EnumSerializer::template get_enum_value<T>(str);
            if (!value) {
                return false;
            }
            t = (T)*value;
            return true;
        } else if constexpr (requires { Serialize<T>::read(std::declval<detail::ArchiveReadAdapter<Base> &>(), std::declval<T &>()); }) {
            // Use Serialize<T> specialization (new system)
            detail::ArchiveReadAdapter<Base> adapter(*static_cast<DeSerializer<Base> *>(this));
            if constexpr (sizeof...(args) == 0) {
                // No name argument: Serialize<T>::read expects an object context (it calls value() with names),
                // so if we're reading from an array, we need to enter the object first
                if (!adapter.start_object()) return false;
                bool result = Serialize<T>::read(adapter, t);
                adapter.end_scope();
                return result;
            } else {
                if (!adapter.start_object(args...)) return false;
                bool result = Serialize<T>::read(adapter, t);
                adapter.end_scope();
                return result;
            }
        } else if constexpr (requires { t.rbc_objdeser(*this); }) {
            // Fall back to legacy rbc_objdeser method
            if (!Base::start_object(args...)) return false;
            t.rbc_objdeser(*this);
            Base::end_scope();
            return true;
        } else if constexpr (requires { t.rbc_arrdeser(*this); }) {
            // Fall back to legacy rbc_arrdeser method
            uint64_t size;
            if (!Base::start_array(size, args...)) return false;
            t.rbc_arrdeser(*this);
            Base::end_scope();
            return true;
        }
        // bool
        else if constexpr (std::is_same_v<T, bool>) {
            return Base::read(t, args...);
        }
        // integer
        else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_unsigned_v<T>) {
                auto v = (uint64_t)t;
                bool result = Base::read(v, args...);
                if (result)
                    t = (T)v;
                return result;
            } else {
                auto v = (uint64_t)t;
                bool result = Base::read(v, args...);
                if (result)
                    t = (T)v;
                return result;
            }
        }
        // float
        else if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, luisa::half>) {
            auto v = (double)t;
            bool result = Base::read(v, args...);
            if (result)
                t = (T)v;
            return result;
        }
        // vector
        else if constexpr (luisa::is_vector_v<T>) {
            constexpr size_t dim = luisa::vector_dimension_v<T>;
            using EleType = luisa::vector_element_t<T>;
            // float vector
            if constexpr (std::is_floating_point_v<EleType> || std::is_same_v<EleType, luisa::half>) {
                uint64_t size{};
                if (!Base::start_array(size, args...)) return false;
                auto end_scope = vstd::scope_exit([&] {
                    Base::end_scope();
                });
                if (size != dim) return false;
                for (size_t i = 0; i < dim; ++i) {
                    auto ele = (double)t[i];
                    bool result = Base::read(ele);
                    if (result) {
                        t[i] = (EleType)ele;
                    } else {
                        return false;
                    }
                }
                return true;
            }
            // int vector
            else if constexpr (std::is_integral_v<EleType>) {
                uint64_t size{};
                if (!Base::start_array(size, args...)) return false;
                auto end_scope = vstd::scope_exit([&] {
                    Base::end_scope();
                });
                if (size != dim) return false;
                for (size_t i = 0; i < dim; ++i) {
                    if constexpr (std::is_unsigned_v<EleType>) {
                        auto ele = (uint64_t)t[i];
                        bool result = Base::read(ele);
                        if (result) {
                            t[i] = (EleType)ele;
                        } else {
                            return false;
                        }
                    } else {
                        auto ele = (int64_t)t[i];
                        bool result = Base::read(ele);
                        if (result) {
                            t[i] = (EleType)ele;
                        } else {
                            return false;
                        }
                    }
                }
                return true;
            }
            // bool vector
            else if constexpr (std::is_same_v<bool, EleType>) {
                uint64_t size{};
                if (!Base::start_array(size, args...)) return false;
                auto end_scope = vstd::scope_exit([&] {
                    Base::end_scope();
                });
                if (size != dim) return false;
                for (size_t i = 0; i < dim; ++i) {
                    if (!Base::read(t[i])) {
                        return false;
                    }
                }
                return true;
            } else {
                static_assert(luisa::always_false_v<T>, "Invalid vector type.");
            }
        } else if constexpr (concepts::is_matrix_v<T>) {
            constexpr size_t dim = luisa::matrix_dimension_v<T>;
            uint64_t size;
            if (!Base::start_array(size, args...)) return false;
            auto end_scope = vstd::scope_exit([&] {
                Base::end_scope();
            });
            if (size != dim * dim) return false;
            for (size_t x = 0; x < dim; ++x)
                for (size_t y = 0; y < dim; ++y) {
                    auto i = x + y * dim;
                    double ele;
                    bool result = Base::read(ele);
                    if (result) {
                        t.cols[x][y] = ele;
                    } else {
                        return false;
                    }
                }
            return true;
        }
        // string
        else if constexpr (std::is_same_v<T, luisa::string>) {
            return Base::read(t, args...);
        }
        // kv map
        else if constexpr (rbc::detail::is_unordered_map<T>::value) {
            uint64_t size;
            if (!Base::start_array(size, args...)) return false;
            auto end_scope = vstd::scope_exit([&] {
                Base::end_scope();
            });

            if constexpr (requires {
                              t.try_emplace(std::declval<luisa::string>(), std::declval<typename rbc::detail::is_unordered_map<T>::ValueType>());
                          })

            {
                t.reserve(size);
                typename rbc::detail::is_unordered_map<T>::ValueType value;
                luisa::string key;
                if (!this->_load(key)) return false;
                if (!this->_load(value)) return false;
                t.try_emplace(
                    std::move(key),
                    std::move(value));
            } else {
                static_assert(luisa::always_false_v<T>, "Invalid HashMap key-value type.");
            }
            return true;
        }
        // duck type
        else if constexpr (rbc::detail::is_vector<T>::value) {
            uint64_t size{};
            if (!Base::start_array(size, args...)) return false;
            auto end_scope = vstd::scope_exit([&] {
                Base::end_scope();
            });
            if constexpr (requires { t.reserve(size); }) {
                t.reserve(size);
            }
            for (uint64_t i = 0; i < size; ++i) {
                auto &v = t.emplace_back();
                if (!this->_load(v)) return false;
            }
            return true;
        } else if constexpr (std::is_same_v<T, vstd::Guid>) {
            luisa::string guid_str;
            if (!Base::read(guid_str, args...)) return false;
            if (guid_str.size() != 22 && guid_str.size() != 32) return false;
            auto g = vstd::Guid::TryParseGuid(guid_str);
            if (!g) return false;
            t = *g;
            return true;
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid deserialize type.");
        }
    }
};
using JsonSerializer = Serializer<JsonWriter>;
using JsonDeSerializer = DeSerializer<JsonReader>;
using ArchiveWrite = detail::ArchiveWriteAdapter<JsonWriter>;
using ArchiveRead = detail::ArchiveReadAdapter<JsonReader>;

template<typename T>
struct is_serializer {
    static constexpr bool value = false;
};
template<typename T>
struct is_deserializer {
    static constexpr bool value = false;
};
template<typename T>
struct is_serializer<Serializer<T>> {
    static constexpr bool value = true;
};
template<typename T>
struct is_deserializer<DeSerializer<T>> {
    static constexpr bool value = true;
};
template<typename T>
static constexpr bool is_serializer_v = is_serializer<T>::value;
template<typename T>
static constexpr bool is_deserializer_v = is_deserializer<T>::value;

// Default Serialize<T> implementations that delegate to rbc_objser/rbc_arrser for backward compatibility
// These are provided as a convenience - users can still specialize Serialize<T> to override them
namespace detail {
// Helper to create default Serialize<T> for types with rbc_objser
template<typename T>
struct DefaultSerializeImpl {
    // Check if type has rbc_objser/rbc_arrser methods
    template<typename Writer>
    static constexpr bool has_objser() {
        return requires(T const &t, Writer &w) { t.rbc_objser(w); };
    }

    template<typename Writer>
    static constexpr bool has_arrser() {
        return requires(T const &t, Writer &w) { t.rbc_arrser(w); };
    }

    template<typename Reader>
    static constexpr bool has_objdeser() {
        return requires(T &t, Reader &r) { t.rbc_objdeser(r); };
    }

    template<typename Reader>
    static constexpr bool has_arrdeser() {
        return requires(T &t, Reader &r) { t.rbc_arrdeser(r); };
    }
};

}// namespace detail

// Macro to help users create default Serialize<T> specializations that delegate to rbc_objser

// Usage: RBC_DEFAULT_SERIALIZE(MyType, JsonSerializer, JsonDeSerializer)
#define RBC_DEFAULT_SERIALIZE(Type, WriterType, ReaderType)                                                 \
    namespace rbc {                                                                                         \
    template<>                                                                                              \
    struct Serialize<Type> {                                                                                \
        static void write(detail::ArchiveWriteAdapter<WriterType> &w, const Type &v) {                      \
            if constexpr (detail::DefaultSerializeImpl<Type>::template has_objser<WriterType>()) {          \
                w.writer.start_object();                                                                    \
                v.rbc_objser(w.writer);                                                                     \
                w.writer.add_last_scope_to_object();                                                        \
            } else if constexpr (detail::DefaultSerializeImpl<Type>::template has_arrser<WriterType>()) {   \
                w.writer.start_array();                                                                     \
                v.rbc_arrser(w.writer);                                                                     \
                w.writer.add_last_scope_to_object();                                                        \
            }                                                                                               \
        }                                                                                                   \
        static bool read(detail::ArchiveReadAdapter<ReaderType> &r, Type &v) {                              \
            if constexpr (detail::DefaultSerializeImpl<Type>::template has_objdeser<ReaderType>()) {        \
                if (!r.reader.start_object()) return false;                                                 \
                v.rbc_objdeser(r.reader);                                                                   \
                r.reader.end_scope();                                                                       \
                return true;                                                                                \
            } else if constexpr (detail::DefaultSerializeImpl<Type>::template has_arrdeser<ReaderType>()) { \
                uint64_t size;                                                                              \
                if (!r.reader.start_array(size)) return false;                                              \
                v.rbc_arrdeser(r.reader);                                                                   \
                r.reader.end_scope();                                                                       \
                return true;                                                                                \
            }                                                                                               \
            return false;                                                                                   \
        }                                                                                                   \
    };                                                                                                      \
    }

}// namespace rbc