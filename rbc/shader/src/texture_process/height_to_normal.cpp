#include <luisa/std.hpp>

using namespace luisa::shader;

float sobel_x(float3x3 data) {
    return data[0][0] * -1.0f +
           data[0][1] * -2.0f +
           data[0][2] * -1.0f +
           data[2][0] * 1.0f +
           data[2][1] * 2.0f +
           data[2][2] * 1.0f;
}

float sobel_y(float3x3 data) {
    return data[0][0] * -1.0f +
           data[1][0] * -2.0f +
           data[2][0] * -1.0f +
           data[0][2] * 1.0f +
           data[1][2] * 2.0f +
           data[2][2] * 1.0f;
}

[[kernel_2d(16, 8)]] int kernel(
    Image<float> &height_img,
    Image<float> &normal_img,
    float height_scale) {

    auto id = int2(dispatch_id().xy);
    auto size = int2(dispatch_size().xy);

    float3x3 r;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            auto sample_id = id + int2(x, y);
            sample_id = clamp(sample_id, 0, int2(size - 1));
            r[x + 1][y + 1] = height_img.read(sample_id).x * height_scale;
        }
    }

    float gx = sobel_x(r);
    float gy = sobel_y(r);

    float3 normal = normalize(float3(-gx, -gy, 1.0f));
    normal_img.write(id, float4(normal * 0.5f + 0.5f, 1.0f));
    return 0;
}
