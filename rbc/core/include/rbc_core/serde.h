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
#include <rbc_core/enum_serializer.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/type_traits.h>
namespace rbc {
namespace concepts {

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
    t.add(std::declval<char const *>());
    // object
    t.add(std::declval<bool>(), std::declval<char const *>());
    t.add(std::declval<int64_t>(), std::declval<char const *>());
    t.add(std::declval<uint64_t>(), std::declval<char const *>());
    t.add(std::declval<double>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<int64_t const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<uint64_t const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<double const>>(), std::declval<char const *>());
    t.add_arr(std::declval<luisa::span<bool const>>(), std::declval<char const *>());
    t.add(std::declval<char const *>(), std::declval<char const *>());

    std::same_as<decltype(t.write_to()), luisa::BinaryBlob>;
};
template<typename T>
concept SerReader = requires(T t) {
    std::same_as<decltype(t.last_key()), luisa::string_view>;
    std::same_as<bool, decltype(t.start_array(lvalue_declval<uint64_t>()))>;
    std::same_as<bool, decltype(t.start_object())>;
    t.end_scope();
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
};
}// namespace concepts
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
template <typename K>
struct is_vector {
    static constexpr bool value = false;
};
template <typename Ele>
struct is_vector<luisa::vector<Ele>> {
    static constexpr bool value = true;
};
template<typename T>
static constexpr bool serializable_native_type_v = std::is_integral_v<T> || std::is_floating_point_v<T> || luisa::is_vector_v<T> || luisa::is_matrix_v<T> || detail::is_unordered_map<T>::value || std::is_same_v<std::decay_t<T>, luisa::string> || std::is_same_v<T, vstd::Guid> || std::is_same_v<std::decay_t<T>, luisa::string_view> || std::is_same_v<T, luisa::half> || std::is_same_v<T, bool> || is_vector<T>::value || std::is_enum_v<T>;
template<typename T, typename Ser>
static constexpr bool serializable_struct_type_v =
    requires { std::declval<T>().rbc_objser(lvalue_declval<Ser>()); } ||
    requires { std::declval<T>().rbc_arrser(lvalue_declval<Ser>()); };
template<typename T, typename DeSer>
static constexpr bool deserializable_struct_type_v =
    requires { std::declval<T>().rbc_objdeser(lvalue_declval<DeSer>()); } ||
    requires { std::declval<T>().rbc_arrdeser(lvalue_declval<DeSer>()); };
}// namespace detail
template<concepts::SerWriter Base>
struct Serializer : public Base {
    vstd::VEngineMallocVisitor _alloc_callback;
    vstd::StackAllocator _alloc;
    template<typename... Args>
        requires(luisa::is_constructible_v<Base, Args && ...>)
    Serializer(Args &&...args)
        : Base(std::forward<Args>(args)...),
          _alloc(4096, &_alloc_callback, 2) {
    }

    template<typename T, typename... Args>
        requires(detail::serializable_native_type_v<T> || detail::serializable_struct_type_v<T, Serializer<Base>>)
    void _store(T const &t, Args... args) {
        // custom type
        if constexpr (std::is_enum_v<T>) {
            static_assert(rbc_rtti_detail::is_rtti_type<T>::value);
            auto strv = EnumSerializer::template get_enum_value_name<T>(luisa::to_underlying(t));
            auto chunk = _alloc.allocate(strv.size() + 1);
            auto ptr = (char *)(chunk.handle + chunk.offset);
            std::memcpy(ptr, strv.data(), strv.size());
            ptr[strv.size()] = 0;
            Base::add(ptr, args...);
        } else if constexpr (requires { t.rbc_objser(*this); }) {
            Base::start_object();
            t.rbc_objser(*this);
            Base::add_last_scope_to_object(args...);
        } else if constexpr (requires { t.rbc_arrser(*this); }) {
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
        } else if constexpr (luisa::is_matrix_v<T>) {
            constexpr size_t dim = luisa::matrix_dimension_v<T>;
            double arr[dim * dim];
            for (size_t x = 0; x < dim; ++x)
                for (size_t y = 0; y < dim; ++y) {
                    arr[x + y * dim] = t.cols[y][x];
                }
            Base::add_arr(luisa::span<double const>{arr, dim * dim}, args...);
        }
        // string
        else if constexpr (std::is_same_v<std::decay_t<T>, char const *> || std::is_same_v<std::decay_t<T>, char *>) {
            Base::add((char const *)t, args...);
        } else if constexpr (requires { std::is_same_v<decltype(t.c_str()), char const *>; }) {
            Base::add(t.c_str(), args...);
        }
        // kv map
        else if constexpr (detail::is_unordered_map<T>::value) {
            Base::start_object();
            for (auto &&i : t) {
                if constexpr (requires { i.first.c_str(); }) {
                    this->_store(i.second, i.first.c_str());
                } else {
                    static_assert(luisa::always_false_v<T>, "Invalid HashMap key-value type.");
                }
            }
            Base::add_last_scope_to_object(args...);
        }
        // duck type
        else if constexpr (requires {t.data(); t. size(); }) {
            auto i = t.data();
            auto end = i + t.size();
            Base::start_array();
            while (i != end) {
                this->_store(*i);
                ++i;
            }
            Base::add_last_scope_to_object(args...);

        } else if constexpr (requires { t.begin() ; t.end(); }) {
            auto i = t.begin();
            auto end = i.end();
            Base::start_array();
            while (i != end) {
                this->_store(*i);
                ++i;
            }
            Base::add_last_scope_to_object(args...);
        } else if constexpr (std::is_same_v<T, vstd::Guid>) {
            auto chunk = _alloc.allocate(23);
            auto ptr = (char *)(chunk.handle + chunk.offset);
            t.to_base64(ptr);
            ptr[22] = 0;
            // TODO
            Base::add(ptr, args...);
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid serialize type.");
        }
    }
    void clear() {
        _alloc.clear();
    }
    template<typename T>
    luisa::BinaryBlob serialize(char const *name, T const &t) {
        _alloc.clear();
        _store(t, name);
        return Base::write_to();
    }
};

template<concepts::SerReader Base>
struct DeSerializer : public Base {
    template<typename... Args>
        requires(luisa::is_constructible_v<Base, Args && ...>)
    DeSerializer(Args &&...args)
        : Base(std::forward<Args>(args)...) {
    }
    template<typename T, typename... Args>
        requires(detail::serializable_native_type_v<T> || detail::deserializable_struct_type_v<T, DeSerializer<Base>>)
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
        } else if constexpr (requires { t.rbc_objdeser(*this); }) {
            if (!Base::start_object(args...)) return false;
            t.rbc_objdeser(*this);
            Base::end_scope();
            return true;
        } else if constexpr (requires { t.rbc_arrdeser(*this); }) {
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
        } else if constexpr (luisa::is_matrix_v<T>) {
            constexpr size_t dim = luisa::matrix_dimension_v<T>;
            for (size_t x = 0; x < dim; ++x)
                for (size_t y = 0; y < dim; ++y) {
                    auto ele = (double)t[i];
                    bool result = Base::read(ele);
                    if (result) {
                        t[x + y * dim] = ele;
                    } else {
                        return false;
                    }
                }
            return true;
        }
        // string
        else if constexpr (std::is_same_v<std::decay_t<T>, luisa::string>) {
            return Base::read(t, args...);
        }
        // kv map
        else if constexpr (detail::is_unordered_map<T>::value) {
            if (!Base::start_object(args...)) return false;
            auto end_scope = vstd::scope_exit([&] {
                Base::end_scope();
            });

            if constexpr (requires {
                              t.try_emplace(std::declval<luisa::string>(), std::declval<typename detail::is_unordered_map<T>::ValueType>());
                          })

            {
                typename detail::is_unordered_map<T>::ValueType value;
                if (!this->_load(value)) return false;
                t.try_emplace(
                    luisa::string{std::move(Base::last_key())},
                    std::move(value));
            } else {
                static_assert(luisa::always_false_v<T>, "Invalid HashMap key-value type.");
            }
            return true;
        }
        // duck type
        else if constexpr (requires { t.emplace_back(); }) {
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

}// namespace rbc