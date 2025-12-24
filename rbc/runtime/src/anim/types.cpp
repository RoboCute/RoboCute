#include "rbc_anim/types.h"

namespace rbc {

}// namespace rbc

bool rbc::Serialize<rbc::AnimFloat4x4>::write(rbc::ArchiveWrite &w, const rbc::AnimFloat4x4 &v) {
    float f[4];
    for (auto col : v.cols) {
        ozz::math::StorePtr(col, f);
        w.value<float>(f[0]);
        w.value<float>(f[1]);
        w.value<float>(f[2]);
        w.value<float>(f[3]);
    }
    return true;
}
bool rbc::Serialize<rbc::AnimFloat4x4>::read(rbc::ArchiveRead &r, rbc::AnimFloat4x4 &v) {
    float f[4];
    for (auto &col : v.cols) {
        r.value<float>(f[0]);
        r.value<float>(f[1]);
        r.value<float>(f[2]);
        r.value<float>(f[3]);
        col = ozz::math::simd_float4::LoadPtr(f);
    }
    return true;
}
