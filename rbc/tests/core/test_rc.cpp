#include "rbc_test.hpp"
#include <rbc_core/rc.h>

namespace rbc::test {
struct DummyRC : public rbc::RCBase {
    int a = 1;
};

void observe(rbc::RCWeak<DummyRC> weak_dummy) {
    auto locked = weak_dummy.lock();
    if (!locked.is_empty()) {
        expect(static_cast<bool>(locked->a == 1));
        // first weak ref
        expect(static_cast<bool>(weak_dummy.ref_count_weak() == 3));
    } else {
        LUISA_INFO("Lock Fail");
        expect(static_cast<bool>(weak_dummy.ref_count_weak() == 0));
    }
}

suite<"Core|RC"> CoreRCTestSuite = [] {

    "rc"_test = [] {
        rbc::RC<DummyRC> ref_rc;

        {
            ref_rc = rbc::RC<DummyRC>::New();
            expect(static_cast<bool>(ref_rc->a == 1));
            expect(static_cast<bool>(ref_rc.ref_count() == 1));
            expect(static_cast<bool>(ref_rc.ref_count_weak() == 0));

            rbc::RCWeak weak_ref{ref_rc.get()};
            // first counter (+ add_ref)
            // weak_ref stay alive (+ add_ref)
            expect(static_cast<bool>(weak_ref.ref_count_weak() == 2));

            observe(weak_ref);
        }
        // weak_ref released
        // only one to keep it alive
        expect(static_cast<bool>(ref_rc.ref_count_weak() == 1));
        {
            // new one added
            rbc::RCWeak weak_ref{ref_rc.get()};
            expect(static_cast<bool>(ref_rc.ref_count_weak() == 2));
            observe(weak_ref);
            // next weak_ref
            ref_rc.reset();// release the pointer
        }
        rbc::RCWeak weak_ref{ref_rc.get()};
        observe(weak_ref);// lock fail
    };
}; // suite

} // namespace rbc::test
