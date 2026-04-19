#include "rbc_anim/types.h"

namespace rbc {

bool Serialize<AnimFloat4x4>::write(ArchiveWrite &w, const AnimFloat4x4 &v) {
    w.start_array();
    float f[4];
    for (const auto &col : v.cols) {
        ozz::math::StorePtr(col, f);
        for (auto fi : f) {
            w.value(fi);
        }
    }
    w.end_array("data");
    return true;
}

bool Serialize<AnimFloat4x4>::read(ArchiveRead &r, AnimFloat4x4 &v) {
    size_t size;
    r.start_array(size, "data");
    float f[4];
    for (auto &col : v.cols) {
        for (auto &fi : f) {
            r.value(fi);
        }
        col = ozz::math::simd_float4::LoadPtr(f);
    }
    r.end_scope();
    return true;
}

} // namespace rbc
