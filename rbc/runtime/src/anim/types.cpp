#include "rbc_anim/types.h"

namespace rbc {

}// namespace rbc

void rbc::Serialize<rbc::AnimFloat4x4>::write(rbc::ArchiveWrite &w, const rbc::AnimFloat4x4 &v) {
    w.start_array();
    float f[4];
    for (auto col : v.cols) {
        ozz::math::StorePtr(col, f);
        w.value<float>(f[0]);
        w.value<float>(f[1]);
        w.value<float>(f[2]);
        w.value<float>(f[3]);
    }
    w.end_object("data");
}
bool rbc::Serialize<rbc::AnimFloat4x4>::read(rbc::ArchiveRead &r, rbc::AnimFloat4x4 &v) {
    size_t size;
    r.start_array(size, "data");
    float f[4];
    for (auto &col : v.cols) {
        r.value<float>(f[0]);
        r.value<float>(f[1]);
        r.value<float>(f[2]);
        r.value<float>(f[3]);
        col = ozz::math::simd_float4::LoadPtr(f);
    }
    r.end_scope();
    return true;
}
