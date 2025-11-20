#include <rbc_core//shared_atomic_mutex.h>
#include <luisa/core/logging.h>

namespace rbc {

shared_atomic_mutex::shared_atomic_mutex()
    : _bitfield(0) {
}

// Acquire the unique lock, first waiting until all shared locks are unlocked.
void shared_atomic_mutex::lock() {
    // Increment the unique lock count (which is the number of threads waiting for a unique lock).
    // Incrementing immediately, whether actively uniquely locked or not, blocks any new thread asking for a new shared lock.
    _bitfield.fetch_add(one_unique_thread);
    acquire_unique();
}

void shared_atomic_mutex::unlock() {
    LUISA_DEBUG_ASSERT(num_unique_locks() > 0 && is_unique_locked());
    _bitfield.fetch_sub(one_unique_flag + one_unique_thread);
}

void shared_atomic_mutex::upgrade() {
    static_assert(one_unique_thread > one_shared_thread, "This section of code assumes one_unique_thread > one_shared_thread for subtraction");
    // increment the unique lock count and remove this thread from the shared lock thread count
    _bitfield.fetch_add(one_unique_thread - one_shared_thread);
    acquire_unique();
}

void shared_atomic_mutex::lock_shared() {
    // Unlike the unique lock, don't immediately increment the shared count (wait until there are no unique lock requests before adding).
    bitfield_t oldval = _bitfield.load();
    bitfield_t newval = oldval;
    do {
        // Proceed if any number of shared locks and no unique locks are waiting or active.
        any_shared_no_unique(oldval);
        // New value is expected value with a new shared lock (increment the shared lock count).
        newval = oldval + one_shared_thread;
        LUISA_INTRIN_PAUSE();
        // If _bitfield==oldval (there are no unique locks) then store newval in _bitfield (add a shared lock).
        // Otherwise update oldval with the latest value of _bitfield and run the test loop again.
    } while ((
        !_bitfield.compare_exchange_weak(oldval, newval, std::memory_order::relaxed, std::memory_order::relaxed)));
}

void shared_atomic_mutex::unlock_shared() {
    assert(num_shared_locks() > 0);
    _bitfield.fetch_sub(one_shared_thread);
}

shared_atomic_mutex::val_t shared_atomic_mutex::num_shared_locks() {
    const auto mask = _bitfield.load() & num_shared_mask;
    return val_t(mask >> num_shared_bitshift);
}

shared_atomic_mutex::val_t shared_atomic_mutex::num_unique_locks() {
    const auto mask = _bitfield.load() & num_unique_mask;
    return val_t(mask >> num_unique_bitshift);
}

bool shared_atomic_mutex::is_unique_locked() {
    const auto mask = _bitfield.load() & unique_flag_mask;
    return (mask >> unique_flag_bitshift) != 0;
}

void shared_atomic_mutex::acquire_unique() {
    bitfield_t oldval = _bitfield;
    bitfield_t newval = oldval;
    do {
        // Proceed if there are no shared locks and the unique lock is available (unique lock flag is 0).
        no_shared_no_unique(oldval);
        // Set the unique lock flag to 1.
        newval = oldval + one_unique_flag;
        LUISA_INTRIN_PAUSE();
        // If _bitfield==oldval (there are no active shared locks and no thread has a unique lock) then store newval in _bitfield (get the unique lock).
        // Otherwise update oldval with the latest value of _bitfield and run the test loop again.
    } while ((
        !_bitfield.compare_exchange_weak(oldval, newval, std::memory_order::relaxed, std::memory_order::relaxed)));
}

}// namespace rbc