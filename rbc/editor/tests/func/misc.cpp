#include "test_util.h"
#include "RBCEditorRuntime/infra/misc.h"

TEST_SUITE("editor") {
    using namespace rbc::test;
    TEST_CASE("func::misc") {
        luisa::float2 xy = {200, 300};
        luisa::float2 res = {1000, 1000};
        luisa::float2 exp_uv = {0.2f, 0.3f};
        luisa::float2 exp_ndc = {-0.6f, -0.4f};
        auto uv = rbc::screen2uv(xy, res);
        auto ndc = rbc::uv2ndc(uv);
        CHECK(feq(uv, exp_uv));
        CHECK(feq(ndc, exp_ndc));
    }
}