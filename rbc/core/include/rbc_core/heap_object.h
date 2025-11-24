#pragma once
#include <luisa/core/spin_mutex.h>
#include <rbc_config.h>
#include <rbc_core/serde.h>
namespace rbc {
struct HeapObjectMeta {
    uint64_t size{};
    uint64_t alignment{};
    vstd::func_ptr_t<void(void *dst)> default_ctor{};
    vstd::func_ptr_t<void(void *dst, void *src)> copy_ctor{};
    vstd::func_ptr_t<void(void *dst, void *src)> move_ctor{};
    vstd::func_ptr_t<void(void *src)> deleter{};
    vstd::func_ptr_t<void(void const *src, JsonSerializer *)> json_writer{};
    vstd::func_ptr_t<void(void *src, JsonDeSerializer *)> json_reader{};
    bool is_trivial_constructible : 1 {false};
    bool is_trivial_copyable : 1 {false};
    bool is_trivial_movable : 1 {false};
    template<typename T>
    void init() {
        size = sizeof(T);
        alignment = alignof(T);
        if constexpr (std::is_trivially_constructible_v<T>) {
            is_trivial_constructible = true;
        } else if constexpr (std::is_default_constructible_v<T>) {
            default_ctor = +[](void *dst) {
                std::construct_at(std::launder(static_cast<T *>(dst)));
            };
        }
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            is_trivial_copyable = true;
        } else if constexpr (std::is_copy_constructible_v<T>) {
            copy_ctor = +[](void *dst, void *src) {
                std::construct_at(std::launder(static_cast<T *>(dst)), *static_cast<T *>(src));
            };
        }
        if constexpr (std::is_trivially_move_constructible_v<T>) {
            is_trivial_movable = true;
        } else if constexpr (std::is_move_constructible_v<T>) {
            move_ctor = +[](void *dst, void *src) {
                std::construct_at(std::launder(static_cast<T *>(dst)), std::move(*static_cast<T *>(src)));
            };
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {
            deleter = +[](void *ptr) {
                std::destroy_at(static_cast<T *>(ptr));
            };
        }
        if constexpr (requires { std::declval<T const>().rbc_objser(lvalue_declval<JsonSerializer>()); }) {
            json_writer = +[](void const *src, JsonSerializer *json_writer) {
                static_cast<T const *>(src)->rbc_objser(*json_writer);
            };
        } else if constexpr (requires { std::declval<JsonSerializer>()._store(std::declval<T>()); }) {
            json_writer = +[](void const *src, JsonSerializer *json_writer) {
                json_writer->_store(*static_cast<T const *>(src));
            };
        }
        if constexpr (requires { lvalue_declval<T>().rbc_objdeser(lvalue_declval<JsonDeSerializer>()); }) {
            json_reader = +[](void *src, JsonDeSerializer *json_reader) {
                static_cast<T *>(src)->rbc_objdeser(*json_reader);
            };
        } else if constexpr (requires { std::declval<JsonDeSerializer>()._load(lvalue_declval<T>()); }) {
            json_reader = +[](void *src, JsonDeSerializer *json_reader) {
                json_reader->_load(*static_cast<T *>(src));
            };
        }
    }
    template<typename T>
    static HeapObjectMeta create() {
        HeapObjectMeta t;
        if constexpr (!std::is_void_v<T>) {
            t.template init<T>();
        }
        return t;
    }
    operator bool() const {
        return size != 0;
    }
    RBC_CORE_API void *allocate() const;
    RBC_CORE_API void deallocate(void *ptr) const;
    RBC_CORE_API void copy(void *dst, void *src) const;
    RBC_CORE_API void move(void *dst, void *src) const;
};
struct HeapObject : HeapObjectMeta {
    luisa::spin_mutex _obj_mtx;
    void *data{};
    void copy_to(void *dst) {
        copy(dst, data);
    }
    void move_to(void *dst) {
        move(dst, data);
    }
    HeapObject() = default;

    template<typename T>
    HeapObject(T &&t) {
        data = luisa::detail::allocator_allocate(HeapObjectMeta::size, HeapObjectMeta::alignment);
        std::construct_at(static_cast<T *>(data), std::forward<T>(t));
        HeapObjectMeta::init<T>();
    }
    HeapObject(HeapObject const &) = delete;
    HeapObject(HeapObject &&) = delete;
    ~HeapObject() {
        if (HeapObjectMeta::deleter) {
            HeapObjectMeta::deleter(data);
        }
        luisa::detail::allocator_deallocate(data, HeapObjectMeta::alignment);
    }
};
}// namespace rbc