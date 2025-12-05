#include <luisa/std.hpp>
#include <spectrum/spectrum.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &in, Image<float> &out,
    Image<float> &temp,
#ifdef TO_BUFFER
    Buffer<float> &out_buffer,
#endif
    uint2 render_resolution, uint frame) {
    auto coord = dispatch_id().xy;
    auto uv = (float2(coord) + 0.5f) / float2(dispatch_size().xy);
    auto sample_id = uint2(float2(render_resolution) * uv);
    auto v = in.read(sample_id);
    v.xyz = spectrum::spectrum_to_tristimulus(v.xyz);
    if (frame > 0) {
        auto old = out.read(coord);
        float alpha = old.w + v.w;
        float3 color = lerp(old.xyz, v.xyz, v.w / max(alpha, 1e-5f));
        v = float4(color, alpha);
    }
#ifdef TO_BUFFER
    auto buffer_id = (coord.x + coord.y * dispatch_size().x) * 3;
    out_buffer.write(buffer_id, v.x);
    out_buffer.write(buffer_id + 1, v.y);
    out_buffer.write(buffer_id + 2, v.z);
#endif
    temp.write(coord, v);
    out.write(coord, v);
    return 0;
}