#include "test_util.h"
#include <rbc_core/rc.h>

struct DummyRC : public rbc::RCBase {
    int a = 1;
};

void observe(rbc::RCWeak<DummyRC> weak_dummy) {
    auto locked = weak_dummy.lock();
    if (!locked.is_empty()) {
        CHECK(locked->a == 1);
        // first weak ref
        CHECK(weak_dummy.ref_count_weak() == 3);
    } else {
        LUISA_INFO("Lock Fail");
        CHECK(weak_dummy.ref_count_weak() == 0);
    }
}

TEST_SUITE("core") {
    TEST_CASE("rc") {
        rbc::RC<DummyRC> ref_rc;

        {
            ref_rc = rbc::RC<DummyRC>::New();
            CHECK(ref_rc->a == 1);
            CHECK(ref_rc.ref_count() == 1);
            CHECK(ref_rc.ref_count_weak() == 0);

            rbc::RCWeak weak_ref{ref_rc.get()};
            // first counter (+ add_ref)
            // weak_ref stay alive (+ add_ref)
            CHECK(weak_ref.ref_count_weak() == 2);

            observe(weak_ref);
        }
        // weak_ref released
        // only one to keep it alive
        CHECK(ref_rc.ref_count_weak() == 1);
        {
            // new one added
            rbc::RCWeak weak_ref{ref_rc.get()};
            CHECK(ref_rc.ref_count_weak() == 2);
            observe(weak_ref);
            // next weak_ref
            ref_rc.reset();// release the pointer
        }
        rbc::RCWeak weak_ref{ref_rc.get()};
        observe(weak_ref);// lock fail
    }
}