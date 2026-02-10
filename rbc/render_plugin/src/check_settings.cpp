#include <rbc_render/generated/pipeline_settings.hpp>
#include <algorithm>
#include <rbc_core/state_map.h>

namespace rbc {

// Helper template to clamp values
template<typename T>
static void clamp_value(T &value, T min, T max) {
    value = std::max(min, std::min(value, max));
}

template<typename T>
static void clamp_vector2(T &v, float min, float max) {
    v.x = std::max(min, std::min(v.x, max));
    v.y = std::max(min, std::min(v.y, max));
}

template<typename T>
static void clamp_vector3(T &v, float min, float max) {
    v.x = std::max(min, std::min(v.x, max));
    v.y = std::max(min, std::min(v.y, max));
    v.z = std::max(min, std::min(v.z, max));
}

template<typename T>
static void clamp_vector4(T &v, float min, float max) {
    v.x = std::max(min, std::min(v.x, max));
    v.y = std::max(min, std::min(v.y, max));
    v.z = std::max(min, std::min(v.z, max));
    v.w = std::max(min, std::min(v.w, max));
}
// Check and clamp ToneMappingParameters
static void check_tone_mapping_parameters(ToneMappingParameters &params) {
    // hdr_display_multiplier: 1e-3f ~ 100.0f
    clamp_value(params.hdr_display_multiplier, 1e-3f, 100.0f);
    // hdr_paper_white: 80.0f ~ 1000.0f (typical max display luminance)
    clamp_value(params.hdr_paper_white, 80.0f, 1000.0f);
}

// Check and clamp ACESParameters
static void check_aces_parameters(ACESParameters &params) {
    // temperature: 1000.f ~ 15000.f
    clamp_value(params.temperature, 1000.0f, 15000.0f);
    // tint: -1 ~ 1
    clamp_value(params.tint, -1.0f, 1.0f);
    // hueShift: -100 ~ 100
    clamp_value(params.hueShift, -100.0f, 100.0f);
    // saturation: -100 ~ 200
    clamp_value(params.saturation, -100.0f, 200.0f);
    // contrast: -100 ~ 100
    clamp_value(params.contrast, -100.0f, 100.0f);

    // Channel mixer values: -200 ~ 200
    clamp_value(params.mixerRedOutRedIn, -200.0f, 200.0f);
    clamp_value(params.mixerRedOutGreenIn, -200.0f, 200.0f);
    clamp_value(params.mixerRedOutBlueIn, -200.0f, 200.0f);
    clamp_value(params.mixerGreenOutRedIn, -200.0f, 200.0f);
    clamp_value(params.mixerGreenOutGreenIn, -200.0f, 200.0f);
    clamp_value(params.mixerGreenOutBlueIn, -200.0f, 200.0f);
    clamp_value(params.mixerBlueOutRedIn, -200.0f, 200.0f);
    clamp_value(params.mixerBlueOutGreenIn, -200.0f, 200.0f);
    clamp_value(params.mixerBlueOutBlueIn, -200.0f, 200.0f);

    // lift: xyz color, w: -1 ~ 1
    clamp_vector3(params.lift, 0.0f, 1.0f);
    clamp_value(params.lift.w, -1.0f, 1.0f);

    // gamma: xyz color, w: -1 ~ 1
    clamp_vector3(params.gamma, 0.0f, 1.0f);
    clamp_value(params.gamma.w, -1.0f, 1.0f);

    // gain: xyz color, w: -1 ~ 1
    clamp_vector3(params.gain, 0.0f, 1.0f);
    clamp_value(params.gain.w, -1.0f, 1.0f);

    // colorFilter: xyz color, w: 0 ~ 5
    clamp_vector3(params.colorFilter, 0.0f, 1.0f);
    clamp_value(params.colorFilter.w, 0.0f, 5.0f);

    // Check nested tone mapping parameters
    check_tone_mapping_parameters(params.tone_mapping);
}

// Check and clamp LpmDispatchParameters
static void check_lpm_parameters(LpmDispatchParameters &params) {
    // saturation: -1 ~ 1 (vector3)
    clamp_vector3(params.saturation, -1.0f, 1.0f);
    // crosstalk: color values 0 ~ 1 (vector3)
    clamp_vector3(params.crosstalk, 0.0f, 1.0f);
    // displayMinLuminance: >= 0
    params.displayMinLuminance = std::max(params.displayMinLuminance, 0.0f);
    // displayMaxLuminance: >= displayMinLuminance
    params.displayMaxLuminance = std::max(params.displayMaxLuminance, params.displayMinLuminance);
}

// Check and clamp ToneMappingSettings
static void check_tone_mapping_settings(ToneMappingSettings &settings) {
    check_lpm_parameters(settings.lpm);
    check_aces_parameters(settings.aces);
}

// Check and clamp DisplaySettings
static void check_display_settings(DisplaySettings &settings) {
    // gamma: 0.1f ~ 10.0f
    clamp_value(settings.gamma, 0.1f, 10.0f);
    // chromatic_aberration: 0 ~ 0.05
    clamp_value(settings.chromatic_aberration, 0.0f, 0.05f);
}

// Check and clamp ExposureSettings
static void check_exposure_settings(ExposureSettings &settings) {
    // filtering: 1 ~ 99 (float2 representing min/max percentiles)
    clamp_vector2(settings.filtering, 1.0f, 99.0f);
    // Ensure filtering.x <= filtering.y
    settings.filtering.x = std::min(settings.filtering.x, settings.filtering.y);

    // minLuminance: -9 ~ 8.99
    clamp_value(settings.minLuminance, -9.0f, 8.99f);
    // maxLuminance: -8.99 ~ 9
    clamp_value(settings.maxLuminance, -8.99f, 9.0f);
    // Ensure minLuminance <= maxLuminance
    settings.minLuminance = std::min(settings.minLuminance, settings.maxLuminance);
    // globalExposure: 1e-3f ~ 256
    clamp_value(settings.globalExposure, 1e-3f, 256.0f);
}

// Check and clamp PathTracerSettings
static void check_path_tracer_settings(PathTracerSettings &settings) {
    // offline_spp: 1 ~ 4
    clamp_value(settings.offline_spp, 1u, 4u);
    // offline_origin_bounce: 1 ~ 4
    clamp_value(settings.offline_origin_bounce, 1u, 4u);
    // offline_indirect_bounce: 2 ~ 8
    clamp_value(settings.offline_indirect_bounce, 2u, 8u);
}

// Check and clamp DistortionSettings
static void check_distortion_settings(DistortionSettings &settings) {
    // scale: 0.01f ~ 5.0f
    clamp_value(settings.scale, 0.01f, 5.0f);
    // intensity: -100 ~ 100
    clamp_value(settings.intensity, -100.0f, 100.0f);
    // intensity_multiplier: 0 ~ 1
    clamp_vector2(settings.intensity_multiplier, 0, 1.0f);
    // center: -1 ~ 1
    clamp_vector2(settings.center, -1.0f, 1.0f);
}

// Check and clamp SkySettings
static void check_sky_settings(SkySettings &settings) {
    // sky_angle: no specific range in pipeline_controller
    // sky_max_lum: > 0
    settings.sky_max_lum = std::max(settings.sky_max_lum, 0.0f);
    // sky_color: 0 ~ 1
    settings.sky_color = max(settings.sky_color, float3(0.0f));
    // sun_color: 0 ~ 1
    clamp_vector3(settings.sun_color, 0.0f, 1.0f);
    // sun_intensity: >= 0
    settings.sun_intensity = std::max(settings.sun_intensity, 0.0f);
    // sun_angle: typical range 0.1 ~ 10 degrees, but no strict limit in code
    settings.sun_angle = std::clamp(settings.sun_angle, 0.0f, 2.0f * pi);
}
void clamp_render_settings(StateMap &map) {
    // Check SkySettings
    if (auto sky_settings = map.read_if<SkySettings>()) {
        check_sky_settings(*sky_settings);
    }

    // Check ToneMappingSettings (includes LpmDispatchParameters and ACESParameters)
    if (auto tone_mapping_settings = map.read_if<ToneMappingSettings>()) {
        check_tone_mapping_settings(*tone_mapping_settings);
    }

    // Check DisplaySettings
    if (auto display_settings = map.read_if<DisplaySettings>()) {
        check_display_settings(*display_settings);
    }

    // Check ExposureSettings
    if (auto exposure_settings = map.read_if<ExposureSettings>()) {
        check_exposure_settings(*exposure_settings);
    }

    // Check PathTracerSettings
    if (auto path_tracer_settings = map.read_if<PathTracerSettings>()) {
        check_path_tracer_settings(*path_tracer_settings);
    }

    // Check DistortionSettings
    if (auto distortion_settings = map.read_if<DistortionSettings>()) {
        check_distortion_settings(*distortion_settings);
    }
}
}// namespace rbc
