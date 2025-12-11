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
namespace rbc {
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
template<typename K>
struct is_vector {
    static constexpr bool value = false;
};
template<typename Ele>
struct is_vector<luisa::vector<Ele>> {
    static constexpr bool value = true;
};
template<typename T>
static constexpr bool serializable_native_type_v = std::is_integral_v<T> || std::is_floating_point_v<T> || luisa::is_vector_v<T> || concepts::is_matrix_v<T> || detail::is_unordered_map<T>::value || std::is_same_v<std::decay_t<T>, luisa::string> || std::is_same_v<T, vstd::Guid> || std::is_same_v<std::decay_t<T>, luisa::string_view> || std::is_same_v<T, luisa::half> || std::is_same_v<T, bool> || is_vector<T>::value || std::is_enum_v<T>;
template<typename T, typename Ser>
static constexpr bool serializable_struct_type_v =
    requires { std::declval<T>().rbc_objser(lvalue_declval<Ser>()); } ||
    requires { std::declval<T>().rbc_arrser(lvalue_declval<Ser>()); };
template<typename T, typename DeSer>
static constexpr bool deserializable_struct_type_v =
    requires { std::declval<T>().rbc_objdeser(lvalue_declval<DeSer>()); } ||
    requires { std::declval<T>().rbc_arrdeser(lvalue_declval<DeSer>()); };
}// namespace detail
namespace concepts {
template<typename T, typename Ser>
concept SerializableType = detail::serializable_native_type_v<T> || detail::serializable_struct_type_v<T, Ser>;
template<typename T, typename DeSer>
concept DeSerializableType = detail::serializable_native_type_v<T> || detail::deserializable_struct_type_v<T, DeSer>;
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
        else if constexpr (detail::is_unordered_map<T>::value) {
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
        } else if constexpr (detail::is_vector<T>::value) {
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
        else if constexpr (detail::is_unordered_map<T>::value) {
            uint64_t size;
            if (!Base::start_array(size, args...)) return false;
            auto end_scope = vstd::scope_exit([&] {
                Base::end_scope();
            });

            if constexpr (requires {
                              t.try_emplace(std::declval<luisa::string>(), std::declval<typename detail::is_unordered_map<T>::ValueType>());
                          })

            {
                t.reserve(size);
                typename detail::is_unordered_map<T>::ValueType value;
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
        else if constexpr (detail::is_vector<T>::value) {
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
}// namespace rbc