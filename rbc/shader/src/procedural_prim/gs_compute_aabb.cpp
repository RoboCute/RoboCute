#include <luisa/std.hpp>
#include <procedural_prim/gaussian.hpp>
using namespace luisa::shader;

/// Kernel entry point for computing Gaussian AABBs
[[kernel_1d(128)]] int kernel(
    Buffer<AABB> &output_buffer,
    Buffer<GaussianProbe> &probe_buffer) {
    uint32 probe_idx = dispatch_id().x;

    AABB aabb = compute_gaussian_aabb(probe_buffer, probe_idx);
    output_buffer.write(probe_idx, aabb);

    return 0;
}
