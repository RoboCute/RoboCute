#pragma once
#include <luisa/core/spin_mutex.h>
#include <luisa/vstl/common.h>
#include <luisa/core/binary_io.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/serde.h>
#include <rbc_config.h>
#include <rbc_core/type_info.h>
namespace rbc {

struct RBC_CORE_API StateMap {
public:
    StateMap() = default;
    void init_json(luisa::string_view json);
    StateMap(StateMap const &) = delete;
    StateMap(StateMap &&) = delete;
    StateMap &operator=(StateMap const &) = delete;
    StateMap &operator=(StateMap &&) = delete;
    ~StateMap();

protected:
    _EXPORT_STD template<class _Ty>
    static _Ty& lvalue_declval() noexcept {
        static_assert(false, "Calling declval is ill-formed, see N4950 [declval]/2.");
    }

    struct RBC_CORE_API HeapObject {
        luisa::spin_mutex _obj_mtx;
        uint64_t size{};
        uint64_t alignment{};
        void *data{};
        vstd::func_ptr_t<void(void *dst, void *src)> copy_ctor{};
        vstd::func_ptr_t<void(void *src)> deleter{};
        vstd::func_ptr_t<void(void *src, Serializer<rbc::JsonWriter> *)> json_writer{};
        vstd::func_ptr_t<void(void *src, DeSerializer<rbc::JsonReader> *)> json_reader{};
        HeapObject() = default;
        template<typename T>
        void init() {
            size = sizeof(T);
            alignment = alignof(T);
            if constexpr (std::is_copy_constructible_v<T>) {
                copy_ctor = +[](void *dst, void *src) {
                    std::construct_at(static_cast<T *>(dst), *static_cast<T *>(src));
                };
            }
            if constexpr (!std::is_trivially_destructible_v<T>) {
                deleter = +[](void *ptr) {
                    std::destroy_at(static_cast<T *>(ptr));
                };
            }
            if constexpr (requires { lvalue_declval<T>().rbc_objser(lvalue_declval<Serializer<rbc::JsonWriter>>()); }) {
                json_writer = +[](void *src, Serializer<rbc::JsonWriter> *json_writer) {
                    static_cast<T *>(src)->rbc_objser(*json_writer);
                };
            }
            if constexpr (requires { lvalue_declval<T>().rbc_objdeser(lvalue_declval<DeSerializer<rbc::JsonReader>>()); }) {
                json_reader = +[](void *src, DeSerializer<rbc::JsonReader> *json_writer) {
                    static_cast<T *>(src)->rbc_objdeser(*json_writer);
                };
            }
        }
        template<typename T>
        HeapObject(T &&t) {
            data = luisa::detail::allocator_allocate(size, alignment);
            std::construct_at(static_cast<T *>(data), std::forward<T>(t));
            this->template init<T>();
        }
        HeapObject(HeapObject const &) = delete;
        HeapObject(HeapObject &&) = delete;
        ~HeapObject() {
            if (deleter) {
                deleter(data);
            }
            luisa::detail::allocator_deallocate(data, alignment);
        }
    };
    vstd::HashMap<rbc::TypeInfo, HeapObject> _map;
    mutable luisa::spin_mutex _map_mtx;
    vstd::optional<DeSerializer<rbc::JsonReader>> _json_reader;
    void _deser(TypeInfo const &type_info, HeapObject &heap_obj);
    static void _log_err_no_copy(luisa::string_view name);
public:

    template<concepts::RTTIType T>
        requires(std::is_default_constructible_v<T>)
    T &read_mut() {
        _map_mtx.lock();
        auto iter = _map.try_emplace(rbc::TypeInfo::get<T>());
        _map_mtx.unlock();
        HeapObject &heap_obj = iter.first.value();
        std::lock_guard lck{heap_obj._obj_mtx};
        if (!heap_obj.data) {
            heap_obj.template init<T>();
            heap_obj.data = luisa::detail::allocator_allocate(heap_obj.size, heap_obj.alignment);
            std::construct_at(static_cast<T *>(heap_obj.data));
            _deser(iter.first.key(), heap_obj);
        }
        return *static_cast<T *>(heap_obj.data);
    }
    template<concepts::RTTIType T>
        requires(std::is_default_constructible_v<T>)
    T const &read() {
        return this->template read_mut<T>();
    }
    template<concepts::RTTIType T>
        requires(std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>)
    T read_safe() {
        vstd::Storage<T> storage;
        auto ptr = reinterpret_cast<T *>(storage.c);
        _map_mtx.lock();
        auto iter = _map.try_emplace(rbc::TypeInfo::get<T>());
        _map_mtx.unlock();
        HeapObject &heap_obj = iter.first.value();
        std::lock_guard lck{heap_obj._obj_mtx};
        if (!heap_obj.data) {
            heap_obj.template init<T>();
            heap_obj.data = luisa::detail::allocator_allocate(heap_obj.size, heap_obj.alignment);
            std::construct_at(static_cast<T *>(heap_obj.data));
            _deser(iter.first.key(), heap_obj);
        }
        if (!heap_obj.copy_ctor) {
            _log_err_no_copy(iter.first.key().name());
        }
        heap_obj.copy_ctor(ptr, heap_obj.data);
        return std::move(*ptr);
    }
    template<concepts::RTTIType T>
        requires(!std::is_reference_v<T>)
    void write(T &&t) {
        _map_mtx.lock();
        auto iter = _map.try_emplace(rbc::TypeInfo::get<T>());
        _map_mtx.unlock();
        HeapObject &heap_obj = iter.first.value();
        std::lock_guard lck{heap_obj._obj_mtx};
        if (!heap_obj.data) {
            heap_obj.template init<T>();
            heap_obj.data = luisa::detail::allocator_allocate(heap_obj.size, heap_obj.alignment);
        }
        std::construct_at(static_cast<T *>(heap_obj.data), std::forward<T>(t));
    }
    luisa::BinaryBlob serialize_to_json();
};
}// namespace rbc
