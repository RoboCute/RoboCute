#pragma once
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <rbc_core/shared_atomic_mutex.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/core/spin_mutex.h>
#include <luisa/core/logging.h>
namespace rbc {
using RCCounterType = uint64_t;
struct RCBase;
// constants
inline static constexpr RCCounterType kRCCounterUniqueFlag = 1 << 31;
inline static constexpr RCCounterType kRCCounterMax = kRCCounterUniqueFlag - 1;

// operations
inline static RCCounterType rc_ref_count(const std::atomic<RCCounterType> &counter) {
    return counter.load(std::memory_order_relaxed) & ~kRCCounterUniqueFlag;
}
inline static bool rc_is_unique(RCCounterType counter) {
    return (counter & kRCCounterUniqueFlag) != 0;
}
inline static RCCounterType rc_add_ref(std::atomic<RCCounterType> &counter) {
    RCCounterType old = counter.load(std::memory_order_relaxed);
    LUISA_DEBUG_ASSERT(!rc_is_unique(old) && "try to add ref on a unique object");
    while (!counter.compare_exchange_weak(
        old,
        old + 1,
        std::memory_order_relaxed)) {
        LUISA_DEBUG_ASSERT(!rc_is_unique(old) && "try to add ref on a unique object");
    }
    return old + 1;
}
inline static RCCounterType rc_weak_lock(std::atomic<RCCounterType> &counter) {
    for (RCCounterType old = counter.load(std::memory_order_relaxed); old != 0;) {
        LUISA_DEBUG_ASSERT(!rc_is_unique(old));
        if (counter.compare_exchange_weak(
                old,
                old + 1,
                std::memory_order_relaxed)) {
            return old;
        }
    }
    return 0;
}
inline static RCCounterType rc_add_ref_unique(std::atomic<RCCounterType> &counter) {
    RCCounterType old = counter.load(std::memory_order_relaxed);
    LUISA_DEBUG_ASSERT(old == 0 && "try to add ref on a non-unique object");
    while (!counter.compare_exchange_weak(
        old,
        kRCCounterUniqueFlag | 1,
        std::memory_order_relaxed)) {
        LUISA_DEBUG_ASSERT(old == 0 && "try to add ref on a non-unique object");
    }
    return kRCCounterUniqueFlag;
}
inline static RCCounterType rc_release_unique(std::atomic<RCCounterType> &counter) {
    RCCounterType old = counter.load(std::memory_order_relaxed);
    LUISA_DEBUG_ASSERT(rc_is_unique(old) && "try to release a non-unique object");
    while (!counter.compare_exchange_weak(
        old,
        0,
        std::memory_order_relaxed)) {
        LUISA_DEBUG_ASSERT(rc_is_unique(old) && "try to release a non-unique object");
    }
    return 0;
}
inline static RCCounterType rc_release(std::atomic<RCCounterType> &counter) {
    RCCounterType old = counter.fetch_sub(1, std::memory_order_release);
    if (rc_is_unique(old)) {
        return 0;
    } else {
        return old - 1;
    }
}

// weak block
struct RCWeakRefCounter {
    inline RCCounterType ref_count() const {
        return _ref_count.load(std::memory_order_relaxed);
    }
    inline void add_ref() {
        _ref_count.fetch_add(1, std::memory_order_relaxed);
    }
    inline void release() {
        LUISA_DEBUG_ASSERT(_ref_count.load(std::memory_order_relaxed) > 0);
        if (_ref_count.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            luisa::delete_with_allocator(this);
        }
    }
    inline void notify_dead() {
        LUISA_DEBUG_ASSERT(_is_alive.load(std::memory_order_relaxed) == true);
        _is_alive.store(false, std::memory_order_relaxed);
    }
    inline bool is_alive() const {
        return _is_alive.load(std::memory_order_relaxed);
    }
    inline void lock_for_use() {
        _delete_mutex.lock_shared();
    }
    inline void unlock_for_use() {
        _delete_mutex.unlock_shared();
    }
    inline void lock_for_delete() {
        _delete_mutex.lock();
    }
    inline void unlock_for_delete() {
        _delete_mutex.unlock();
    }

private:
    std::atomic<RCCounterType> _ref_count = 0;
    rbc::shared_atomic_mutex _delete_mutex = {};
    std::atomic<bool> _is_alive = true;
};
inline static RCWeakRefCounter *rc_get_weak_ref_released() {
    return reinterpret_cast<RCWeakRefCounter *>(size_t(-1));
}
inline static bool rc_is_weak_ref_released(RCWeakRefCounter *counter) {
    return reinterpret_cast<size_t>(counter) == size_t(-1);
}
inline static RCCounterType rc_weak_ref_count(
    std::atomic<RCWeakRefCounter *> &counter) {
    RCWeakRefCounter *weak_counter = counter.load(std::memory_order_relaxed);
    if (!weak_counter || rc_is_weak_ref_released(weak_counter)) {
        return 0;
    } else {
        std::atomic_thread_fence(std::memory_order_acquire);
        return weak_counter->ref_count();
    }
}
inline static RCWeakRefCounter *rc_get_or_new_weak_ref_counter(
    std::atomic<RCWeakRefCounter *> &counter) {
    RCWeakRefCounter *weak_counter = counter.load(std::memory_order_relaxed);
    if (weak_counter) {
        return rc_is_weak_ref_released(weak_counter) ? nullptr : weak_counter;
    } else {
        RCWeakRefCounter *new_counter = luisa::new_with_allocator<RCWeakRefCounter>();
        if (counter.compare_exchange_weak(
                weak_counter,
                new_counter,
                std::memory_order_release)) {
            std::atomic_thread_fence(std::memory_order_acquire);
            // add ref for keep it alive until any weak ref and self released
            new_counter->add_ref();
            return new_counter;
        } else {
            std::atomic_thread_fence(std::memory_order_acquire);
            if (rc_is_weak_ref_released(weak_counter)) {
                // object has been released, delete the new one
                luisa::delete_with_allocator(new_counter);
                return nullptr;
            } else {
                // another thread created a weak counter, delete the new one
                luisa::delete_with_allocator(new_counter);
                return weak_counter;
            }
        }
    }
}
inline static void rc_notify_weak_ref_counter_dead(
    std::atomic<RCWeakRefCounter *> &counter) {
    RCWeakRefCounter *weak_counter = counter.load(std::memory_order_relaxed);

    // take release permissions
    while (!counter.compare_exchange_weak(
        weak_counter,
        rc_get_weak_ref_released(),
        std::memory_order_release)) {
        if (rc_is_weak_ref_released(weak_counter)) {
            // unexpected, another thread released the weak counter
            vstd::unreachable();
        } else {
            // another thread created a weak counter
            LUISA_DEBUG_ASSERT(weak_counter != nullptr);
        }
    }

    // now release the weak counter
    if (weak_counter) {
        // lock for delete
        weak_counter->lock_for_delete();

        // release
        std::atomic_thread_fence(std::memory_order_acquire);
        weak_counter->notify_dead();
        weak_counter->release();

        // unlock for delete
        weak_counter->unlock_for_delete();
    }
}

// concept
template<typename T>
concept ObjectWithRC = requires(const T *const_obj, T *obj) {
    { const_obj->rbc_rc_count() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_add_ref() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_add_ref_unique() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_release_unique() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_weak_lock() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_release() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_weak_ref_count() } -> std::same_as<rbc::RCCounterType>;
    { const_obj->rbc_rc_weak_ref_counter() } -> std::same_as<rbc::RCWeakRefCounter *>;
};
template<typename T>
concept ObjectWithRCDeleter = requires(const T *const_obj, T *obj) {
    { obj->rbc_rc_delete() } -> std::same_as<void>;
};
template<typename From, typename To>
concept RCConvertible = requires() {
    ObjectWithRC<From>;
    ObjectWithRC<To>;
    std::convertible_to<From *, To *>;
};
template<typename From, typename To>
concept RCFromBase = requires() {
    std::is_base_of_v<From, RCBase>;
    std::is_base_of_v<To, RCBase>;
    std::convertible_to<From *, To *>;
};
template<typename From, typename To>
concept RCLegalType = RCConvertible<From, To> || RCFromBase<From, To>;

// deleter traits
template<typename T>
struct RCDeleterTraits {
    inline static void do_delete(T *obj) {
        luisa::delete_with_allocator(obj);
    }
};
template<ObjectWithRCDeleter T>
struct RCDeleterTraits<T> {
    inline static void do_delete(T *obj) {
        obj->rbc_rc_delete();
    }
};

// release helper
template<typename T>
inline void rc_release_with_delete(T *p) {
    LUISA_DEBUG_ASSERT(p != nullptr);
    if (p->rbc_rc_release() == 0) {
        p->rbc_rc_weak_ref_counter_notify_dead();
        RCDeleterTraits<T>::do_delete(p);
    }
}

}// namespace rbc

// interface macros
#define RBC_RC_INTEFACE()                                               \
    virtual rbc::RCCounterType rbc_rc_count() const = 0;                \
    virtual rbc::RCCounterType rbc_rc_add_ref() const = 0;              \
    virtual rbc::RCCounterType rbc_rc_add_ref_unique() const = 0;       \
    virtual rbc::RCCounterType rbc_rc_release_unique() const = 0;       \
    virtual rbc::RCCounterType rbc_rc_weak_lock() const = 0;            \
    virtual rbc::RCCounterType rbc_rc_release() const = 0;              \
    virtual rbc::RCCounterType rbc_rc_weak_ref_count() const = 0;       \
    virtual rbc::RCWeakRefCounter *rbc_rc_weak_ref_counter() const = 0; \
    virtual void rbc_rc_weak_ref_counter_notify_dead() const = 0;
#define RBC_RC_DELETER_INTERFACE() \
    virtual void rbc_rc_delete() = 0;

// impl macros
#define RBC_RC_IMPL_(x)                                                             \
private:                                                                            \
    mutable ::std::atomic<::rbc::RCCounterType> zz_rbc_rc = 0;                      \
    mutable ::std::atomic<::rbc::RCWeakRefCounter *> zz_rbc_weak_counter = nullptr; \
                                                                                    \
    x:                                                                              \
    inline rbc::RCCounterType rbc_rc_count() const {                                \
        return rbc::rc_ref_count(zz_rbc_rc);                                        \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_add_ref() const {                              \
        return rbc::rc_add_ref(zz_rbc_rc);                                          \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_add_ref_unique() const {                       \
        return rbc::rc_add_ref_unique(zz_rbc_rc);                                   \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_release_unique() const {                       \
        return rbc::rc_release_unique(zz_rbc_rc);                                   \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_weak_lock() const {                            \
        return rbc::rc_weak_lock(zz_rbc_rc);                                        \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_release() const {                              \
        return rbc::rc_release(zz_rbc_rc);                                          \
    }                                                                               \
    inline rbc::RCCounterType rbc_rc_weak_ref_count() const {                       \
        return rbc::rc_weak_ref_count(zz_rbc_weak_counter);                         \
    }                                                                               \
    inline rbc::RCWeakRefCounter *rbc_rc_weak_ref_counter() const {                 \
        return rbc::rc_get_or_new_weak_ref_counter(zz_rbc_weak_counter);            \
    }                                                                               \
    inline void rbc_rc_weak_ref_counter_notify_dead() const {                       \
        rbc::rc_notify_weak_ref_counter_dead(zz_rbc_weak_counter);                  \
    }
#define RBC_RC_IMPL RBC_RC_IMPL_(public)
#define RBC_RC_DELETER_IMPL_DEFAULT             \
    inline void rbc_rc_delete() {               \
        luisa::delete_with_allocator(this); \
    }

namespace rbc {

template<typename T>
struct RC;
template<typename T>
struct RCWeak;
template<typename T>
struct RCUnique;
template<typename T>
struct RCWeakLocker;
struct RCBase : public vstd::IOperatorNewBase {
    template<typename T>
    friend struct RC;
    template<typename T>
    friend struct RCWeak;
    template<typename T>
    friend struct RCUnique;
    template<typename T>
    friend struct RCWeakLocker;
    template<typename T>
    friend void rc_release_with_delete(T *p);

    RBC_RC_IMPL_(private)
    RBC_RC_DELETER_IMPL_DEFAULT
public:
    virtual ~RCBase() = default;
};
template<typename T>
struct RC {
    friend struct RCWeakLocker<T>;

    // ctor & dtor
    RC();
    RC(std::nullptr_t);
    RC(T *ptr);
    template<RCLegalType<T> U>
    RC(U *ptr);
    template<RCLegalType<T> U>
    RC(RCUnique<U> &&rhs);
    ~RC();

    // copy & move
    RC(const RC &rhs);
    RC(RC &&rhs);
    template<RCLegalType<T> U>
    RC(const RC<U> &rhs);
    template<RCLegalType<T> U>
    RC(RC<U> &&rhs);

    // assign & move assign
    RC &operator=(std::nullptr_t);
    RC &operator=(T *ptr);
    RC &operator=(const RC &rhs);
    RC &operator=(RC &&rhs);
    template<RCLegalType<T> U>
    RC &operator=(U *ptr);
    template<RCLegalType<T> U>
    RC &operator=(const RC<U> &rhs);
    template<RCLegalType<T> U>
    RC &operator=(RC<U> &&rhs);
    template<RCLegalType<T> U>
    RC &operator=(RCUnique<U> &&rhs);

    // factory
    template<typename... Args>
        requires(luisa::is_constructible_v<T, Args...>)
    static RC New(Args &&...args);

    // getter
    T *get() const;

    // count getter
    RCCounterType ref_count() const;
    RCCounterType ref_count_weak() const;

    // empty
    bool is_empty() const;
    operator bool() const;

    // ops
    void reset();
    void reset(T *ptr);
    template<RCLegalType<T> U>
    void reset(U *ptr);
    void swap(RC &rhs);

    // pointer behaviour
    T *operator->() const;
    T &operator*() const;

    // cast
    template<typename U>
    RC<U> cast_static() const;

private:
    // helper
    void _release();

private:
    T *_ptr = nullptr;
};

template<typename T>
struct RCUnique {
    // ctor & dtor
    RCUnique();
    RCUnique(std::nullptr_t);
    RCUnique(T *ptr);
    template<RCLegalType<T> U>
    RCUnique(U *ptr);
    ~RCUnique();

    // copy & move
    RCUnique(const RCUnique &rhs) = delete;
    RCUnique(RCUnique &&rhs);
    template<RCLegalType<T> U>
    RCUnique(RCUnique<U> &&rhs);

    // assign & move assign
    RCUnique &operator=(std::nullptr_t);
    RCUnique &operator=(T *ptr);
    RCUnique &operator=(const RCUnique &rhs) = delete;
    RCUnique &operator=(RCUnique &&rhs);
    template<RCLegalType<T> U>
    RCUnique &operator=(U *ptr);
    template<RCLegalType<T> U>
    RCUnique &operator=(RCUnique<U> &&rhs);

    // factory
    template<typename... Args>
        requires(luisa::is_constructible_v<T, Args...>)
    static RCUnique New(Args &&...args);

    // getter
    T *get() const;

    // count getter
    RCCounterType ref_count() const;
    RCCounterType ref_count_weak() const;

    // empty
    bool is_empty() const;
    operator bool() const;

    // ops
    void reset();
    void reset(T *ptr);
    template<RCLegalType<T> U>
    void reset(U *ptr);
    T *release();
    void swap(RCUnique &rhs);

    // pointer behaviour
    T *operator->() const;
    T &operator*() const;

private:
    // helper
    void _release();

private:
    T *_ptr = nullptr;
};

template<typename T>
struct RCWeakLocker {
    // ctor & dtor
    RCWeakLocker(T *ptr, RCWeakRefCounter *counter);
    ~RCWeakLocker();

    // copy & move
    RCWeakLocker(const RCWeakLocker &rhs) = delete;
    RCWeakLocker(RCWeakLocker &&rhs);

    // assign & move assign
    RCWeakLocker &operator=(const RCWeakLocker &rhs) = delete;
    RCWeakLocker &operator=(RCWeakLocker &&rhs);

    // is empty
    bool is_empty() const;
    operator bool() const;

    // getter
    T *get() const;

    // pointer behaviour
    T *operator->() const;
    T &operator*() const;

    // lock to RC
    RC<T> rc() const;
    operator RC<T>() const;

private:
    T *_ptr;
    RCWeakRefCounter *_counter;
};

template<typename T>
struct RCWeak {
    // ctor & dtor
    RCWeak();
    RCWeak(std::nullptr_t);
    RCWeak(T *ptr);
    template<RCLegalType<T> U>
    RCWeak(U *ptr);
    template<RCLegalType<T> U>
    RCWeak(const RC<U> &ptr);
    template<RCLegalType<T> U>
    RCWeak(const RCUnique<U> &ptr);
    ~RCWeak();

    // copy & move
    RCWeak(const RCWeak &rhs);
    RCWeak(RCWeak &&rhs);
    template<RCLegalType<T> U>
    RCWeak(const RCWeak<U> &rhs);
    template<RCLegalType<T> U>
    RCWeak(RCWeak<U> &&rhs);

    // assign & move assign
    RCWeak &operator=(std::nullptr_t);
    RCWeak &operator=(T *ptr);
    RCWeak &operator=(const RCWeak &rhs);
    RCWeak &operator=(RCWeak &&rhs);
    template<RCLegalType<T> U>
    RCWeak &operator=(U *ptr);
    template<RCLegalType<T> U>
    RCWeak &operator=(const RCWeak<U> &rhs);
    template<RCLegalType<T> U>
    RCWeak &operator=(RCWeak<U> &&rhs);
    template<RCLegalType<T> U>
    RCWeak &operator=(const RC<U> &rhs);
    template<RCLegalType<T> U>
    RCWeak &operator=(const RCUnique<U> &rhs);

    // unsafe getter
    T *get_unsafe() const;
    RCWeakRefCounter *get_counter() const;

    // count getter
    RCCounterType ref_count_weak() const;

    // lock
    RCWeakLocker<T> lock() const;

    // empty
    bool is_empty() const;
    bool is_expired() const;
    bool is_alive() const;
    operator bool() const;

    // ops
    void reset();
    void reset(T *ptr);
    template<RCLegalType<T> U>
    void reset(U *ptr);
    template<RCLegalType<T> U>
    void reset(const RC<U> &ptr);
    template<RCLegalType<T> U>
    void reset(const RCUnique<U> &ptr);
    void swap(RCWeak &rhs);

    // cast
    template<typename U>
    RCWeak<U> cast_static() const;

    // rbc hash
    static size_t _rbc_hash(const RCWeak &obj);

private:
    // helper
    void _release();
    void _take_weak_ref_counter();

private:
    T *_ptr = nullptr;
    rbc::RCWeakRefCounter *_counter = nullptr;
};
}// namespace rbc

// impl for RC
namespace rbc {
// helper
template<typename T>
inline void RC<T>::_release() {
    rc_release_with_delete(_ptr);
}

// ctor & dtor
template<typename T>
inline RC<T>::RC() {
}
template<typename T>
inline RC<T>::RC(std::nullptr_t) {
}
template<typename T>
inline RC<T>::RC(T *ptr)
    : _ptr(ptr) {
    if (_ptr) {
        _ptr->rbc_rc_add_ref();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T>::RC(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        _ptr = static_cast<T *>(ptr);
        _ptr->rbc_rc_add_ref();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T>::RC(RCUnique<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!rhs.is_empty()) {
        _ptr = rhs.release();
        if (_ptr) {
            _ptr->rbc_rc_add_ref();
        }
    }
}
template<typename T>
inline RC<T>::~RC() {
    reset();
}

// copy & move
template<typename T>
inline RC<T>::RC(const RC &rhs)
    : _ptr(rhs._ptr) {
    if (_ptr) {
        _ptr->rbc_rc_add_ref();
    }
}
template<typename T>
inline RC<T>::RC(RC &&rhs)
    : _ptr(rhs._ptr) {
    rhs._ptr = nullptr;
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T>::RC(const RC<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!rhs.is_empty()) {
        reset(static_cast<T *>(rhs.get()));
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T>::RC(RC<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!rhs.is_empty()) {
        reset(static_cast<T *>(rhs.get()));
        rhs.reset();
    }
}

// assign & move assign
template<typename T>
inline RC<T> &RC<T>::operator=(std::nullptr_t) {
    reset();
    return *this;
}
template<typename T>
inline RC<T> &RC<T>::operator=(T *ptr) {
    reset(ptr);
    return *this;
}
template<typename T>
inline RC<T> &RC<T>::operator=(const RC &rhs) {
    if (this != &rhs) {
        reset(rhs._ptr);
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T> &RC<T>::operator=(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset(ptr);
    return *this;
}
template<typename T>
inline RC<T> &RC<T>::operator=(RC &&rhs) {
    if (this != &rhs) {
        reset();
        _ptr = rhs._ptr;
        rhs._ptr = nullptr;
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T> &RC<T>::operator=(const RC<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (rhs.is_empty()) {
        reset();
    } else {
        reset(static_cast<T *>(rhs.get()));
    }
    return *this;
}

template<typename T>
template<RCLegalType<T> U>
inline RC<T> &RC<T>::operator=(RC<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (rhs.is_empty()) {
        reset();
    } else {
        reset(static_cast<T *>(rhs.get()));
        rhs.reset();
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RC<T> &RC<T>::operator=(RCUnique<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset();
    if (!rhs.is_empty()) {
        _ptr = rhs.release();
        if (_ptr) {
            _ptr->rbc_rc_add_ref();
        }
    }
    return *this;
}

// factory
template<typename T>
template<typename... Args>
    requires(luisa::is_constructible_v<T, Args...>)
inline RC<T> RC<T>::New(Args &&...args) {
    return {luisa::new_with_allocator<T>(std::forward<Args>(args)...)};
}

// getter
template<typename T>
inline T *RC<T>::get() const {
    return _ptr;
}

// count getter
template<typename T>
inline RCCounterType RC<T>::ref_count() const {
    return _ptr ? _ptr->rbc_rc_count() : 0;
}
template<typename T>
inline RCCounterType RC<T>::ref_count_weak() const {
    return _ptr ? _ptr->rbc_rc_weak_ref_count() : 0;
}

// empty
template<typename T>
inline bool RC<T>::is_empty() const {
    return _ptr == nullptr;
}
template<typename T>
inline RC<T>::operator bool() const {
    return !is_empty();
}

// ops
template<typename T>
inline void RC<T>::reset() {
    if (_ptr) {
        _release();
        _ptr = nullptr;
    }
}
template<typename T>
inline void RC<T>::reset(T *ptr) {
    if (_ptr != ptr) {
        reset();

        // add ref to new ptr
        _ptr = ptr;
        if (_ptr) {
            _ptr->rbc_rc_add_ref();
        }
    }
}
template<typename T>
template<RCLegalType<T> U>
inline void RC<T>::reset(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        reset(static_cast<T *>(ptr));
    } else {
        reset();
    }
}
template<typename T>
inline void RC<T>::swap(RC &rhs) {
    if (this != &rhs) {
        T *tmp = _ptr;
        _ptr = rhs._ptr;
        rhs._ptr = tmp;
    }
}

// pointer behaviour
template<typename T>
inline T *RC<T>::operator->() const {
    return _ptr;
}
template<typename T>
inline T &RC<T>::operator*() const {
    return *_ptr;
}

// cast
template<typename T>
template<typename U>
inline RC<U> RC<T>::cast_static() const {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<U>, "when use covariance, T must have virtual destructor for safe delete");
    if (_ptr) {
        return RC<U>(static_cast<U *>(_ptr));
    } else {
        return nullptr;
    }
}

}// namespace rbc

// impl for RCUnique
namespace rbc {
// helper
template<typename T>
inline void RCUnique<T>::_release() {
    rc_release_with_delete(_ptr);
}

// ctor & dtor
template<typename T>
inline RCUnique<T>::RCUnique() {
}
template<typename T>
inline RCUnique<T>::RCUnique(std::nullptr_t) {
}
template<typename T>
template<RCLegalType<T> U>
inline RCUnique<T>::RCUnique(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        _ptr = static_cast<T *>(ptr);
        _ptr->rbc_rc_add_ref_unique();
    }
}
template<typename T>
inline RCUnique<T>::RCUnique(T *ptr)
    : _ptr(ptr) {
    if (_ptr) {
        _ptr->rbc_rc_add_ref_unique();
    }
}
template<typename T>
inline RCUnique<T>::~RCUnique() {
    reset();
}

// copy & move
template<typename T>
inline RCUnique<T>::RCUnique(RCUnique &&rhs)
    : _ptr(rhs._ptr) {
    rhs._ptr = nullptr;
}
template<typename T>
template<RCLegalType<T> U>
inline RCUnique<T>::RCUnique(RCUnique<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!rhs.is_empty()) {
        reset(static_cast<T *>(rhs.release()));
    }
}

// assign & move assign
template<typename T>
inline RCUnique<T> &RCUnique<T>::operator=(std::nullptr_t) {
    reset();
    return *this;
}
template<typename T>
inline RCUnique<T> &RCUnique<T>::operator=(T *ptr) {
    reset(ptr);
    return *this;
}
template<typename T>
inline RCUnique<T> &RCUnique<T>::operator=(RCUnique &&rhs) {
    if (this != &rhs) {
        reset();
        _ptr = rhs._ptr;
        rhs._ptr = nullptr;
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCUnique<T> &RCUnique<T>::operator=(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset(ptr);
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCUnique<T> &RCUnique<T>::operator=(RCUnique<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (rhs.is_empty()) {
        reset();
    } else {
        reset(static_cast<T *>(rhs.release()));
    }
    return *this;
}

// factory
template<typename T>
template<typename... Args>
    requires(luisa::is_constructible_v<T, Args...>)
inline RCUnique<T> RCUnique<T>::New(Args &&...args) {
    return {luisa::new_with_allocator<T>(std::forward<Args>(args)...)};
}

// getter
template<typename T>
inline T *RCUnique<T>::get() const {
    return _ptr;
}

// count getter
template<typename T>
inline RCCounterType RCUnique<T>::ref_count() const {
    return _ptr ? _ptr->rbc_rc_count() : 0;
}
template<typename T>
inline RCCounterType RCUnique<T>::ref_count_weak() const {
    return _ptr ? _ptr->rbc_rc_weak_ref_count() : 0;
}

// empty
template<typename T>
inline bool RCUnique<T>::is_empty() const {
    return _ptr == nullptr;
}
template<typename T>
inline RCUnique<T>::operator bool() const {
    return !is_empty();
}

// ops
template<typename T>
inline void RCUnique<T>::reset() {
    if (_ptr) {
        _release();
        _ptr = nullptr;
    }
}
template<typename T>
inline void RCUnique<T>::reset(T *ptr) {
    if (_ptr != ptr) {
        // release old ptr
        if (_ptr) {
            _release();
        }

        // add ref to new ptr
        _ptr = ptr;
        if (_ptr) {
            _ptr->rbc_rc_add_ref_unique();
        }
    }
}
template<typename T>
template<RCLegalType<T> U>
inline void RCUnique<T>::reset(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        reset(static_cast<T *>(ptr));
    } else {
        reset();
    }
}
template<typename T>
inline T *RCUnique<T>::release() {
    if (_ptr) {
        T *tmp = _ptr;
        _ptr->rbc_rc_release_unique();
        _ptr = nullptr;
        return tmp;
    } else {
        return nullptr;
    }
}
template<typename T>
inline void RCUnique<T>::swap(RCUnique &rhs) {
    if (this != &rhs) {
        T *tmp = _ptr;
        _ptr = rhs._ptr;
        rhs._ptr = tmp;
    }
}

// pointer behaviour
template<typename T>
inline T *RCUnique<T>::operator->() const {
    return _ptr;
}
template<typename T>
inline T &RCUnique<T>::operator*() const {
    return *_ptr;
}

}// namespace rbc

// impl for RCWeakLocker
namespace rbc {
// ctor & dtor
template<typename T>
inline RCWeakLocker<T>::RCWeakLocker(T *ptr, RCWeakRefCounter *counter)
    : _ptr(ptr), _counter(counter) {
    if (counter && counter->is_alive()) {
        _counter->lock_for_use();
        if (counter->is_alive()) {// success lock
            return;
        } else {// failed lock
            _counter->unlock_for_use();
        }
    }

    // failed lock, reset ptr
    _ptr = nullptr;
    _counter = nullptr;
}
template<typename T>
inline RCWeakLocker<T>::~RCWeakLocker() {
    if (_ptr) {
        _counter->unlock_for_use();
    }
}

// copy & move
template<typename T>
inline RCWeakLocker<T>::RCWeakLocker(RCWeakLocker &&rhs)
    : _ptr(rhs._ptr), _counter(rhs._counter) {
    rhs._ptr = nullptr;
    rhs._counter = nullptr;
}

// assign & move assign
template<typename T>
inline RCWeakLocker<T> &RCWeakLocker<T>::operator=(RCWeakLocker &&rhs) {
    if (this != &rhs) {
        if (_ptr) {
            _counter->unlock_for_use();
        }

        _ptr = rhs._ptr;
        _counter = rhs._counter;
        rhs._ptr = nullptr;
        rhs._counter = nullptr;
    }
    return *this;
}

// is empty
template<typename T>
inline bool RCWeakLocker<T>::is_empty() const {
    return _ptr == nullptr;
}
template<typename T>
inline RCWeakLocker<T>::operator bool() const {
    return !is_empty();
}

// getter
template<typename T>
inline T *RCWeakLocker<T>::get() const {
    return _ptr;
}

// pointer behaviour
template<typename T>
inline T *RCWeakLocker<T>::operator->() const {
    return _ptr;
}
template<typename T>
inline T &RCWeakLocker<T>::operator*() const {
    return *_ptr;
}

// lock to RC
template<typename T>
inline RC<T> RCWeakLocker<T>::rc() const {
    RC<T> result;
    if (_ptr) {
        auto lock_result = _ptr->rbc_rc_weak_lock();
        if (lock_result != 0) {
            result._ptr = _ptr;
        }
    }
    return result;
}
template<typename T>
inline RCWeakLocker<T>::operator RC<T>() const {
    return rc();
}

}// namespace rbc

// impl for RCWeak
namespace rbc {
// helper
template<typename T>
inline void RCWeak<T>::_release() {
    LUISA_DEBUG_ASSERT(_ptr != nullptr);
    _counter->release();
}
template<typename T>
inline void RCWeak<T>::_take_weak_ref_counter() {
    LUISA_DEBUG_ASSERT(_ptr != nullptr);
    _counter = _ptr->rbc_rc_weak_ref_counter();
    LUISA_DEBUG_ASSERT(_counter != nullptr);
    _counter->add_ref();
}

// ctor & dtor
template<typename T>
inline RCWeak<T>::RCWeak() {
}
template<typename T>
inline RCWeak<T>::RCWeak(std::nullptr_t) {
}
template<typename T>
inline RCWeak<T>::RCWeak(T *ptr)
    : _ptr(ptr) {
    if (_ptr) {
        _take_weak_ref_counter();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T>::RCWeak(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        _ptr = static_cast<T *>(ptr);
        _take_weak_ref_counter();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T>::RCWeak(const RC<U> &ptr)
    : _ptr(static_cast<T *>(ptr.get())) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (_ptr) {
        _take_weak_ref_counter();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T>::RCWeak(const RCUnique<U> &ptr)
    : _ptr(static_cast<T *>(ptr.get())) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (_ptr) {
        _take_weak_ref_counter();
    }
}
template<typename T>
inline RCWeak<T>::~RCWeak() {
    reset();
}

// copy & move
template<typename T>
inline RCWeak<T>::RCWeak(const RCWeak &rhs)
    : _ptr(rhs._ptr), _counter(rhs._counter) {
    if (_counter) {
        _counter->add_ref();
    }
}
template<typename T>
inline RCWeak<T>::RCWeak(RCWeak &&rhs)
    : _ptr(rhs._ptr), _counter(rhs._counter) {
    rhs._ptr = nullptr;
    rhs._counter = nullptr;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T>::RCWeak(const RCWeak<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (rhs.is_alive()) {
        _ptr = static_cast<T *>(rhs.get_unsafe());
        _counter = rhs.get_counter();
        _counter->add_ref();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T>::RCWeak(RCWeak<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (rhs.is_alive()) {
        _ptr = static_cast<T *>(rhs.get_unsafe());
        _counter = rhs.get_counter();
        _counter->add_ref();
        rhs.reset();
    }
}

// assign & move assign
template<typename T>
inline RCWeak<T> &RCWeak<T>::operator=(std::nullptr_t) {
    reset();
    return *this;
}
template<typename T>
inline RCWeak<T> &RCWeak<T>::operator=(T *ptr) {
    reset(ptr);
    return *this;
}
template<typename T>
inline RCWeak<T> &RCWeak<T>::operator=(const RCWeak &rhs) {
    if (this != &rhs) {
        reset(rhs._ptr);
    }
    return *this;
}
template<typename T>
inline RCWeak<T> &RCWeak<T>::operator=(RCWeak &&rhs) {
    if (this != &rhs) {
        reset();
        _ptr = rhs._ptr;
        _counter = rhs._counter;
        rhs._ptr = nullptr;
        rhs._counter = nullptr;
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T> &RCWeak<T>::operator=(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset(ptr);
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T> &RCWeak<T>::operator=(const RCWeak<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset();
    if (rhs.is_alive()) {
        _ptr = static_cast<T *>(rhs.get_unsafe());
        _counter = rhs.get_counter();
        _counter->add_ref();
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T> &RCWeak<T>::operator=(RCWeak<U> &&rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset();
    if (rhs.is_alive()) {
        _ptr = static_cast<T *>(rhs.get_unsafe());
        _counter = rhs.get_counter();
        _counter->add_ref();
        rhs.reset();
    }
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T> &RCWeak<T>::operator=(const RC<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset(rhs);
    return *this;
}
template<typename T>
template<RCLegalType<T> U>
inline RCWeak<T> &RCWeak<T>::operator=(const RCUnique<U> &rhs) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    reset(rhs);
    return *this;
}

// unsafe getter
template<typename T>
inline T *RCWeak<T>::get_unsafe() const {
    return _ptr;
}
template<typename T>
inline RCWeakRefCounter *RCWeak<T>::get_counter() const {
    return _counter;
}

// count getter
template<typename T>
inline RCCounterType RCWeak<T>::ref_count_weak() const {
    return _counter ? _counter->ref_count() : 0;
}

// lock
template<typename T>
inline RCWeakLocker<T> RCWeak<T>::lock() const {
    return {_ptr, _counter};
}

// empty
template<typename T>
inline bool RCWeak<T>::is_empty() const {
    return _ptr == nullptr;
}
template<typename T>
inline bool RCWeak<T>::is_expired() const {
    return !is_alive();
}
template<typename T>
inline bool RCWeak<T>::is_alive() const {
    if (_ptr == nullptr) { return false; }
    return _counter->is_alive();
}
template<typename T>
inline RCWeak<T>::operator bool() const {
    return is_alive();
}

// ops
template<typename T>
inline void RCWeak<T>::reset() {
    if (_ptr) {
        _release();
        _ptr = nullptr;
        _counter = nullptr;
    }
}
template<typename T>
inline void RCWeak<T>::reset(T *ptr) {
    if (_ptr != ptr) {
        // release old ptr
        if (_ptr) {
            _release();
        }

        // add ref to new ptr
        _ptr = ptr;
        if (_ptr) {
            _take_weak_ref_counter();
        }
    }
}
template<typename T>
template<RCLegalType<T> U>
inline void RCWeak<T>::reset(U *ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (ptr) {
        reset(static_cast<T *>(ptr));
    } else {
        reset();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline void RCWeak<T>::reset(const RC<U> &ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!ptr.is_empty()) {
        reset(static_cast<T *>(ptr.get()));
    } else {
        reset();
    }
}
template<typename T>
template<RCLegalType<T> U>
inline void RCWeak<T>::reset(const RCUnique<U> &ptr) {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<T>, "when use covariance, T must have virtual destructor for safe delete");
    if (!ptr.is_empty()) {
        reset(static_cast<T *>(ptr.get()));
    } else {
        reset();
    }
}
template<typename T>
inline void RCWeak<T>::swap(RCWeak &rhs) {
    if (this != &rhs) {
        T *tmp_ptr = _ptr;
        rbc::RCWeakRefCounter *tmp_counter = _counter;
        _ptr = rhs._ptr;
        _counter = rhs._counter;
        rhs._ptr = tmp_ptr;
        rhs._counter = tmp_counter;
    }
}

// cast
template<typename T>
template<typename U>
inline RCWeak<U> RCWeak<T>::cast_static() const {
    static_assert(std::is_same_v<U, T> || std::has_virtual_destructor_v<U>, "when use covariance, T must have virtual destructor for safe delete");
    if (_ptr) {
        if (auto locker = lock()) {
            return RCWeak<U>(static_cast<U *>(_ptr));
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

// rbc hash
template<typename T>
inline size_t RCWeak<T>::_rbc_hash(const RCWeak &obj) {
    return hash_combine(
        luisa::hash<T *>()(obj._ptr),
        luisa::hash<rbc::RCWeakRefCounter *>()(obj._counter));
}

}// namespace rbc