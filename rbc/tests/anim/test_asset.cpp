#include "test_util.h"
#include "rbc_anim/types.h"
#include "rbc_core/serde.h"
#include <luisa/core/logging.h>

namespace rbc {

TEST_SUITE("anim") {
    TEST_CASE("asset") {

        rbc::AnimFloat4x4 m;
        float f[4];
        float b = 1.0f;
        for (auto &col : m.cols) {
            for (auto i = 0; i < 4; i++) {
                f[i] = b + (float)(i);
            }
            b += 4.0f;
            col = ozz::math::simd_float4::LoadPtr(f);
        }

        JsonSerializer writer;
        writer._store(m, "anim_float4x4");
        auto json_blob = writer.write_to();
        luisa::string_view json_str{(const char *)json_blob.data(), json_blob.size()};
        LUISA_INFO("Serialized AnimFloat4x4: \n ==== \n {} \n === \n", json_str);

        JsonDeSerializer reader{json_str};

        rbc::AnimFloat4x4 dm;
        LUISA_ASSERT(reader._load(dm, "anim_float4x4"));

        float df[4];
        b = 1.0f;
        for (auto &col : dm.cols) {
            ozz::math::StorePtr(col, df);
            for (auto i = 0; i < 4; i++) {
                CHECK(df[i] == (float)i + b);
            }
            b += 4.0f;
        }
    }
}

}// namespace rbc