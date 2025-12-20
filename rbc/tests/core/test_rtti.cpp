#include "test_util.h"
#include "simple_rtti.h"

TEST_SUITE("core") {
    TEST_CASE("rtti") {
        // registered as rtti type
        CHECK(rbc::is_rtti_type_v<Dummy>);
        // check name
        auto type_info = rbc::TypeInfo::get<Dummy>();
        CHECK(type_info.name() == "Dummy");

        auto md5 = vstd::MD5{luisa::string_view("Dummy")};
    }
}