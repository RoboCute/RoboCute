#include <rbc_render/post_process/aces.h>
#include <rbc_render/pipeline.h>
#include <luisa/vstl/common.h>
#include <rbc_render/utils//color_space.h>

namespace rbc {
namespace aces_detail {
static constexpr uint32_t curve_precision = 128u;
static constexpr uint32_t volume_resolution = 128u;

auto ColorToLift(float4 color) {
    // Shadows
    auto S = float3(color.x, color.y, color.z);
    auto lumLift = S.x * 0.2126f + S.y * 0.7152f + S.z * 0.0722f;
    S = float3(S.x - lumLift, S.y - lumLift, S.z - lumLift);
    auto liftOffset = color.w;
    return float3(S.x + liftOffset, S.y + liftOffset, S.z + liftOffset);
}
auto ColorToGain(float4 color) {
    // Highlights
    auto H = float3(color.x, color.y, color.z);
    auto lumGain = H.x * 0.2126f + H.y * 0.7152f + H.z * 0.0722f;
    H = float3(H.x - lumGain, H.y - lumGain, H.z - lumGain);
    auto gainOffset = color.w + 1.f;
    return float3(H.x + gainOffset, H.y + gainOffset, H.z + gainOffset);
}
auto ColorToInverseGamma(float4 color) {
    // Midtones
    auto M = float3(color.x, color.y, color.z);
    auto lumGamma = M.x * 0.2126f + M.y * 0.7152f + M.z * 0.0722f;
    M = float3(M.x - lumGamma, M.y - lumGamma, M.z - lumGamma);
    auto gammaOffset = color.w + 1.f;
    return float3(
        1.f / max(M.x + gammaOffset, 1e-03f),
        1.f / max(M.y + gammaOffset, 1e-03f),
        1.f / max(M.z + gammaOffset, 1e-03f));
}

void post_process_aces_get_curve(ACESParameters const &desc, float4 *curve_host_data, bool is_hdr) {
    for (auto i : vstd::range(curve_precision)) {
        auto time = (float(i) + 0.0f) / float(curve_precision);
        auto x = desc.hueVsHueCurve.sample_node(time);
        auto y = desc.hueVsSatCurve.sample_node(time);
        auto z = desc.satVsSatCurve.sample_node(time);
        auto w = desc.lumVsSatCurve.sample_node(time);
        curve_host_data[i] = float4(x, y, z, w);
    }
    if (!is_hdr) {
        for (auto i : vstd::range(curve_precision)) {
            auto time = (float(i) + 0.0f) / float(curve_precision);
            auto m = desc.masterCurve.sample_node(time);
            auto r = desc.redCurve.sample_node(time);
            auto g = desc.greenCurve.sample_node(time);
            auto b = desc.blueCurve.sample_node(time);
            curve_host_data[uint(i) + curve_precision] = float4(r, g, b, m);
        }
    }
}

void post_process_aces_get_arg(
    ACESParameters const &desc,
    ACES::Args &args) {

    // hue & sat & con
    auto hue = desc.hueShift / 360.f;        // Remap to [-0.5;0.5]
    auto sat = desc.saturation / 100.f + 1.f;// Remap to [0;2]
    auto con = desc.contrast / 100.f + 1.f;  // Remap to [0;2]
    args.HueSatCon = float4(hue, sat, con, 0.f);

    // temperature & tint
    args.ColorBalance = make_float4(::rbc::white_balance_lms(desc.temperature, desc.tint, desc.use_white_balance_mode), 0.0f);

    // channel mixer
    auto channelMixerR = float3(desc.mixerRedOutRedIn, desc.mixerRedOutGreenIn, desc.mixerRedOutBlueIn);
    auto channelMixerG = float3(desc.mixerGreenOutRedIn, desc.mixerGreenOutGreenIn, desc.mixerGreenOutBlueIn);
    auto channelMixerB = float3(desc.mixerBlueOutRedIn, desc.mixerBlueOutGreenIn, desc.mixerBlueOutBlueIn);
    args.ChannelMixerRed = make_float4(channelMixerR / 100.f, 0.0f);
    args.ChannelMixerGreen = make_float4(channelMixerG / 100.0f, 0.0f);
    args.ChannelMixerBlue = make_float4(channelMixerB / 100.0f, 0.0f);

    // color ring(or trackball)
    args.Lift = make_float4(ColorToLift(desc.lift), 0.f);
    args.InvGamma = make_float4(ColorToInverseGamma(desc.gamma), 0.f);
    args.Gain = make_float4(ColorToGain(desc.gain), 0.f);
    args.ColorFilter = desc.colorFilter;
    args.ColorFilter *= args.ColorFilter.w;

    // tone mode
    args.tone_mode = luisa::to_underlying(desc.tone_mapping.tone_mode);

    // unreal aces
    args.unreal_tone_slope = desc.tone_mapping.unreal_tone_slope;
    args.unreal_tone_toe = desc.tone_mapping.unreal_tone_toe;
    args.unreal_tone_black_clip = desc.tone_mapping.unreal_tone_black_clip;
    args.unreal_tone_shoulder = desc.tone_mapping.unreal_tone_shoulder;
    args.unreal_tone_white_clip = desc.tone_mapping.unreal_tone_white_clip;

    // filmic aces
    args.filmic_tone_shoulder_strength = desc.tone_mapping.filmic_tone_shoulder_strength;
    args.filmic_tone_linear_strength = desc.tone_mapping.filmic_tone_linear_strength;
    args.filmic_tone_linear_angle = desc.tone_mapping.filmic_tone_linear_angle;
    args.filmic_tone_toe_strength = desc.tone_mapping.filmic_tone_toe_strength;
    args.filmic_tone_toe_numerator = desc.tone_mapping.filmic_tone_toe_numerator;
    args.filmic_tone_toe_denominator = desc.tone_mapping.filmic_tone_toe_denominator;
}

}// namespace aces_detail
void ACES::get_curve_texture(
    ACESParameters const &desc,
    Device &device,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer) {
    if (!curve_img) {
        curve_img = device.create_image<float>(PixelStorage::FLOAT4, uint2(aces_detail::curve_precision, 2));
    }
    luisa::vector<float4> host_data;
    host_data.resize(aces_detail::curve_precision * 2u);
    aces_detail::post_process_aces_get_curve(desc, host_data.data(), is_hdr);
    LUISA_ASSUME(host_data.size_bytes() == curve_img.view().size_bytes());
    auto buffer = temp_buffer.allocate_upload_buffer(host_data.size_bytes(), 512);
    std::memcpy(buffer.mapped_ptr(), host_data.data(), host_data.size_bytes());
    cmdlist << curve_img.copy_from(buffer.view);
}
ACES::ACES(
    luisa::fiber::counter &counter,
    bool is_hdr)
    : is_hdr(is_hdr) {
    ShaderManager::instance()->async_load(counter, "post_process/unity_aces_lut.bin", lut3d_shader);
}
ACES::~ACES() {}
void ACES::early_render(
    ACESParameters const &desc,
    Device &device,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer) {
    get_curve_texture(desc, device, cmdlist, temp_buffer);
    using namespace aces_detail;
    if (!lut3d_volume) {
        lut3d_volume = device.create_volume<float>(PixelStorage::FLOAT4, uint3(volume_resolution));
    }
}

void ACES::dispatch(
    ACESParameters const &desc,
    CommandList &cmdlist, bool disable_aces) {
    using namespace aces_detail;
    Args args;
    post_process_aces_get_arg(desc, args);
    cmdlist << (*lut3d_shader)(lut3d_volume, curve_img, args, disable_aces).dispatch(lut3d_volume.size());
}

}// namespace rbc