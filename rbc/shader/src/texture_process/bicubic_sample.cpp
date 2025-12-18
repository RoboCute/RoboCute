#include <luisa/std.hpp>

using namespace luisa::shader;
float4 CubicHermite(float4 A, float4 B, float4 C, float4 D, float t) {
    float t2 = t * t;
    float t3 = t * t * t;
    float4 a = -A / 2.0f + (3.0f * B) / 2.0f - (3.0f * C) / 2.0f + D / 2.0f;
    float4 b = A - (5.0f * B) / 2.0f + 2.0f * C - D / 2.0f;
    float4 c = -A / 2.0f + C / 2.0f;
    float4 d = B;

    return a * t3 + b * t2 + c * t + d;
}
float4 BicubicHermiteTextureSample(
    SampleImage &sample_img,
    float2 P,
    float2 c_textureSize) {
    float2 pixel = P * c_textureSize + 0.5f;

    float2 frac = fract(pixel);
    auto c_onePixel = 1.0f / c_textureSize;
    auto c_twoPixels = c_onePixel * 2.f;
    pixel = floor(pixel) / c_textureSize - float2(c_onePixel / 2.0f);
    auto C00 = sample_img.sample(pixel + float2(-c_onePixel.x, -c_onePixel.x), Filter::POINT, Address::EDGE);
    auto C10 = sample_img.sample(pixel + float2(0.0f, -c_onePixel.y), Filter::POINT, Address::EDGE);
    auto C20 = sample_img.sample(pixel + float2(c_onePixel.x, -c_onePixel.y), Filter::POINT, Address::EDGE);
    auto C30 = sample_img.sample(pixel + float2(c_twoPixels.x, -c_onePixel.y), Filter::POINT, Address::EDGE);

    auto C01 = sample_img.sample(pixel + float2(-c_onePixel.x, 0.0f), Filter::POINT, Address::EDGE);
    auto C11 = sample_img.sample(pixel + float2(0.0f, 0.0f), Filter::POINT, Address::EDGE);
    auto C21 = sample_img.sample(pixel + float2(c_onePixel.x, 0.0), Filter::POINT, Address::EDGE);
    auto C31 = sample_img.sample(pixel + float2(c_twoPixels.x, 0.0), Filter::POINT, Address::EDGE);

    auto C02 = sample_img.sample(pixel + float2(-c_onePixel.x, c_onePixel.y), Filter::POINT, Address::EDGE);
    auto C12 = sample_img.sample(pixel + float2(0.0, c_onePixel.y), Filter::POINT, Address::EDGE);
    auto C22 = sample_img.sample(pixel + float2(c_onePixel.x, c_onePixel.y), Filter::POINT, Address::EDGE);
    auto C32 = sample_img.sample(pixel + float2(c_twoPixels.x, c_onePixel.y), Filter::POINT, Address::EDGE);

    auto C03 = sample_img.sample(pixel + float2(-c_onePixel.x, c_twoPixels.y), Filter::POINT, Address::EDGE);
    auto C13 = sample_img.sample(pixel + float2(0.0f, c_twoPixels.y), Filter::POINT, Address::EDGE);
    auto C23 = sample_img.sample(pixel + float2(c_onePixel.x, c_twoPixels.y), Filter::POINT, Address::EDGE);
    auto C33 = sample_img.sample(pixel + float2(c_twoPixels.x, c_twoPixels.y), Filter::POINT, Address::EDGE);

    auto CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    auto CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    auto CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    auto CP3X = CubicHermite(C03, C13, C23, C33, frac.x);

    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

[[kernel_2d(16, 8)]] int kernel(
    SampleImage &src_img,
    Image<float> &dst_img) {
    auto id = dispatch_id().xy;
    auto size = dispatch_size().xy;
    auto uv = (float2(id) + 0.5f) / float2(size);
    dst_img.write(id, BicubicHermiteTextureSample(src_img, uv, float2(src_img.size())));
    return 0;
}
