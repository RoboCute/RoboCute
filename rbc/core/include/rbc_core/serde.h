#pragma once
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl/type_traits.h>
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/binary_io.h>
#include <luisa/vstl/hash_map.h>
#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>
namespace rbc {
namespace detail {
template<typename T>
struct is_unordered_map {
    static constexpr bool value = false;
};
template<typename K, typename V>
struct is_unordered_map<luisa::unordered_map<K, V>> {
    static constexpr bool value = true;
};
template<typename K, typename V>
struct is_unordered_map<vstd::HashMap<K, V>> {
    static constexpr bool value = true;
};
}// namespace detail
template<typename Base>
struct Serializer : public Base {
    template<typename... Args>
        requires(luisa::is_constructible_v<Base, Args && ...>)
    Serializer(Args &&...args)
        : Base(std::forward<Args>(args)...) {
    }

    template<typename T, typename... Args>
    void _store(T const &t, Args... args) {

        // custom type
        if constexpr (requires { t.rbc_objser(*this); }) {
            Base::start_object();
            t.rbc_objser(*this);
            Base::add_last_scope_to_object(args...);
        } else if constexpr (requires { t.rbc_arrser(*this); }) {
            Base::start_array();
            t.rbc_objser(*this);
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
                    static_assert(luisa::always_false_v<T>, "HashMap's key can only be string.");
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
        } else {
            static_assert(luisa::always_false_v<T>, "Invalid serialize type.");
        }
    }
    template<typename T>
    luisa::BinaryBlob serialize(char const *name, T const &t) {
        _store(t, name);
        return Base::write_to();
    }
};

}// namespace rbc