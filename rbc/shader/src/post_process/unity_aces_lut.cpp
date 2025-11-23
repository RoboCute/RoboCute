#define LUT3D_USE_REC_2020
#include <post_process/aces.hpp>
#include <post_process/colors.hpp>
using namespace luisa::shader;
#include <post_process/aces_arg.inl>
#include "spectrum/color_space.hpp"

float3 LogGrade(float3 colorLog, Args a) {
    // Contrast feels a lot more natural when done in log rather than doing it in linear
    colorLog = Contrast(colorLog, ACEScc_MIDGRAY, a.HueSatCon.z);
    return colorLog;
}
float3 LinearGrade(float3 colorLinear, Args a, SampleImage &curve_img) {
    colorLinear *= a.ColorFilter.xyz;
    colorLinear = ChannelMixer(colorLinear, a.ChannelMixerRed.xyz, a.ChannelMixerGreen.xyz, a.ChannelMixerBlue.xyz);

    colorLinear = LiftGammaGainHDR(colorLinear, a.Lift.xyz, a.InvGamma.xyz, a.Gain.xyz);

    // Do NOT feed negative values to RgbToHsv or they'll wrap around
    colorLinear = max(float3(0.0), colorLinear);

    float3 hsv = RgbToHsv(colorLinear);

    // Hue Vs Sat
    float satMult;
    satMult = saturate(curve_img.sample(float2(hsv.x, 0.25), Filter::LINEAR_POINT, Address::EDGE).y) * 2.0;

    // // Sat Vs Sat
    satMult *= saturate(curve_img.sample(float2(hsv.y, 0.25), Filter::LINEAR_POINT, Address::EDGE).z) * 2.0;

    // // Lum Vs Sat
    satMult *= saturate(curve_img.sample(float2(Luminance(colorLinear), 0.25), Filter::LINEAR_POINT, Address::EDGE).w) * 2.0;
    // Hue Vs Hue
    float hue = hsv.x + a.HueSatCon.x;
    float offset = saturate(curve_img.sample(float2(hue, 0.25), Filter::LINEAR_POINT, Address::EDGE).x) - 0.5;
    hue += offset;
    hsv.x = RotateHue(hue, 0.0, 1.0);

    colorLinear = HsvToRgb(hsv);
    colorLinear = Saturation(colorLinear, a.HueSatCon.y * satMult);

    return colorLinear;
}

const float3x3 Wide_2_XYZ_MAT =
    {
        0.5441691, 0.2395926, 0.1666943,
        0.2394656, 0.7021530, 0.0583814,
        -0.0023439, 0.0361834, 1.0552183};

const float3x3 BlueCorrect =
    {
        0.9404372683, -0.0183068787, 0.0778696104,
        0.0083786969, 0.8286599939, 0.1629613092,
        0.0005471261, -0.0008833746, 1.0003362486};
const float3x3 BlueCorrectInv =
    {
        1.06318, 0.0233956, -0.0865726,
        -0.0106337, 1.20632, -0.19569,
        -0.000590887, 0.00105248, 0.999538};

float3 ColorGrade(float3 colorLutSpace, SampleImage &curve_img, Args a) {
    float3 colorLinear = LUT_SPACE_DECODE(colorLutSpace);
#ifdef LUT3D_USE_REC_2020
    float3 aces = rec2020_to_ACES_AP0(colorLinear);
#else
    float3 aces = srgb_to_ACES_AP0(colorLinear);
#endif

    // ACEScc (log) space
    float3 acescc = ACES_to_ACEScc(aces);
    acescc = LogGrade(acescc, a);
    aces = ACEScc_to_ACES(acescc);
    // ACEScg (linear) space
    float3 acescg = ACES_to_ACEScg(aces);

    acescg = WhiteBalance(acescg, a.ColorBalance.xyz);

    acescg = LinearGrade(acescg, a, curve_img);
    // Tonemap ODT(RRT(aces))
    aces = ACEScg_to_ACES(acescg);

    // keep rec2020 color space for later tonemapper
#ifdef LUT3D_USE_REC_2020
    colorLinear = ACES_AP0_to_rec2020(aces);
#else
    colorLinear = ACES_AP0_to_srgb(aces);
#endif

    return colorLinear;
}

[[kernel_3d(8, 8, 8)]] int kernel(
    Volume<float> &dst_volume,
    SampleImage &curve_img,
    Args args) {
    float3 graded = ColorGrade(float3(dispatch_id()) / float3(dispatch_size() - 1u), curve_img, args);
    dst_volume.write(dispatch_id(), float4(max(graded, float3(0.f)), 1));
    return 0;
}