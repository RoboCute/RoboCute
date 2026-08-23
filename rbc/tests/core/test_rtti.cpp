#include "rbc_test.hpp"
#include "simple_rtti.h"

namespace rbc::test {
suite<"Core|RTTI"> CoreRTTITestSuite = [] {

    "rtti"_test = [] {
        // registered as rtti type
        expect(static_cast<bool>(rbc::is_rtti_type_v<Dummy>));
        // check name
        auto type_info = rbc::TypeInfo::get<Dummy>();
        expect(static_cast<bool>(type_info.name() == "Dummy"));

        auto md5 = vstd::MD5{luisa::string_view("Dummy")};
    };
}; // suite

} // namespace rbc::test
