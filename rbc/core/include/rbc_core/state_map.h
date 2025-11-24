#pragma once
#include <luisa/core/spin_mutex.h>
#include <luisa/vstl/common.h>
#include <luisa/core/binary_io.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/serde.h>
#include <rbc_config.h>
#include <rbc_core/type_info.h>
#include <rbc_core/heap_object.h>
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

    vstd::HashMap<rbc::TypeInfo, HeapObject> _map;
    mutable luisa::spin_mutex _map_mtx;
    vstd::optional<JsonDeSerializer> _json_reader;
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
    T read_atomic() {
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
        heap_obj.copy_to(ptr);
        return std::move(*ptr);
    }
    template<concepts::RTTIType T>
        requires(std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>)
    T read_atomic_move() {
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
        heap_obj.move_to(ptr);
        return std::move(*ptr);
    }
    template<concepts::RTTIType T>
        requires(!std::is_reference_v<T>)
    void write_atomic(T &&t) {
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
