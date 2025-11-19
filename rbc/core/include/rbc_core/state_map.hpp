#pragma once
#include <luisa/core/spin_mutex.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/json_serde.h>
#include <rbc_config.h>
#include <rbc_core/type.h>
namespace rbc {

struct RBC_CORE_API StateMap {
public:
    StateMap() = default;
    StateMap(StateMap const &) = delete;
    StateMap(StateMap &&) = delete;
    StateMap &operator=(StateMap const &) = delete;
    StateMap &operator=(StateMap &&) = delete;
    ~StateMap();

    template<typename T>
        requires(luisa::is_constructible_v<T>)
    const T &read() const {
        //TODO
        return {};
    }
    template<typename T>
        requires(!std::is_reference_v<T>)
    void write(T &&value) {
        //TODO
    }

    void copy_to(StateMap &dst);

    void save() const;
    void clear();
    void release();

protected:

    struct RBC_CORE_API HeapObject {
        HeapObject() = default;
        ~HeapObject() = default;

        HeapObject duplicate() const;
        void move(const HeapObject &rhs);
        void dtor();
        void dtor_and_free();

        rbc::TypeInfo type;
        uint64_t size = 0;
        uint64_t alignment = 4;
        void *data = nullptr;
        void (*const copier)(void *src, void *dst) = nullptr;
        void (*const mover)(void *src, void *dst) = nullptr;
        void (*const deleter)(void *src) = nullptr;
        void (*const json_writer)(void *src, JsonWriter *w) = nullptr;
        void (*const json_reader)(void *src, JsonReader *r) = nullptr;
    };
    vstd::HashMap<rbc::TypeInfo, HeapObject> _map;
    mutable luisa::spin_mutex _save_mtx;
};
}// namespace rbc
