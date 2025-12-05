import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
from rbc_meta.utils.codegen import cpp_interface_gen, cpp_impl_gen
from pathlib import Path


def codegen_header(header_path: Path, cpp_path: Path):
    LpmColorSpace = tr.enum(
        "rbc::LpmColorSpace",
        REC709=0,
        P3=1,
        REC2020=2,
        Display=3,
    )

    ResourceColorSpace = tr.enum(
        "rbc::ResourceColorSpace",
        Rec709=0,
        AdobeRGB=1,
        P3_D60=2,
        P3_D65=3,
        Rec2020=4,
    )
    LpmDisplayMode = tr.enum(
        "rbc::LpmDisplayMode",
        LDR=0,
        HDR10_2084=1,
        HDR10_SCRGB=2,
        FSHDR_2084=3,
        FSHDR_SCRGB=4,
    )

    NRD_CheckerboardMode = tr.enum(
        "rbc::NRD_CheckerboardMode", OFF=None, BLACK=None, WHITE=None, MAX_NUM=None
    )
    NRD_HitDistanceReconstructionMode = tr.enum(
        "rbc::NRD_HitDistanceReconstructionMode",
        # Probabilistic split at primary hit is not used, hence hit distance is always valid (reconstruction is not needed)
        OFF=None,
        # If hit distance is invalid due to probabilistic sampling, reconstruct using 3x3 neighbors.
        # Probability at primary hit must be clamped to [1/4; 3/4] range to guarantee a sample in this area
        AREA_3X3=None,
        # If hit distance is invalid due to probabilistic sampling, reconstruct using 5x5 neighbors.
        # Probability at primary hit must be clamped to [1/16; 15/16] range to guarantee a sample in this area
        AREA_5X5=None,
        MAX_NUM=None,
    )

    DistortionSettings = tr.struct("rbc::DistortionSettings")
    DistortionSettings.serde_members(
        scale=tr.float,
        intensity=tr.float,
        intensity_multiplier=tr.float2,
        center=tr.float2,
    )
    DistortionSettings.init_member(
        {
            "scale": "1.0",
            "intensity": "0.0",
            "intensity_multiplier": "1, 1",
            "center": "0.0f, 0.0f",
        }
    )

    LpmDispatchParameters = tr.struct("rbc::LpmDispatchParameters")
    LpmDispatchParameters.serde_members(
        shoulder=tr.bool,
        softGap=tr.float,
        hdrMax=tr.float,
        lpmExposure=tr.float,
        contrast=tr.float,
        shoulderContrast=tr.float,
        saturation=tr.float3,
        crosstalk=tr.float3,
        colorSpace=LpmColorSpace,
        displayMode=LpmDisplayMode,
        displayRedPrimary=tr.float2,
        displayGreenPrimary=tr.float2,
        displayBluePrimary=tr.float2,
        displayWhitePoint=tr.float2,
        displayMinLuminance=tr.float,
        displayMaxLuminance=tr.float,
    )
    LpmDispatchParameters.init_member(
        {
            "shoulder": "true",
            "hdrMax": "1847",
            "lpmExposure": "10",
            "shoulderContrast": "1.0",
            "saturation": "0.f, 0.f, 0.f",
            "crosstalk": "1.f, 1.f, 1.f",
            "colorSpace": "rbc::LpmColorSpace::REC2020",
            "displayMinLuminance": "0.001",
            "displayMaxLuminance": "1000",
        }
    )
    ToneMappingParameters = tr.struct("rbc::ToneMappingParameters")
    ToneMappingParameters.serde_members(
        hdr_display_multiplier=tr.float,
        hdr_paper_white=tr.float,
    )
    ToneMappingParameters.init_member(
        {
            "hdr_display_multiplier": "5.0f",
            "hdr_paper_white": "80.0f",
        }
    )
    FrameSettings = tr.struct("rbc::FrameSettings")
    FrameSettings.members(
        to_rec2020_matrix=tr.float3x3,
        render_resolution=tr.uint2,
        display_resolution=tr.uint2,
        display_offset=tr.uint2,
        frame_index=tr.ulong,
        time=tr.double,
        delta_time=tr.float,
        resource_color_space=ResourceColorSpace,
        realtime_rendering=tr.bool,
        offline_capturing=tr.bool,
        albedo_buffer=tr.external_type("luisa::compute::Buffer<float> const*"),
        normal_buffer=tr.external_type("luisa::compute::Buffer<float> const*"),
        radiance_buffer=tr.external_type("luisa::compute::Buffer<float> const*"),
        resolved_img=tr.external_type("luisa::compute::Image<float> const*"),
        dst_img=tr.external_type("luisa::compute::Image<float> const*"),
    )
    FrameSettings.init_member(
        {
            "resource_color_space": "ResourceColorSpace::Rec709",
            "realtime_rendering": "true",
        }
    )
    curve = tr.external_type("Curve")
    ACESParameters = tr.struct("rbc::ACESParameters")
    ACESParameters.members(
        hueVsHueCurve=curve,
        hueVsSatCurve=curve,
        satVsSatCurve=curve,
        lumVsSatCurve=curve,
        redCurve=curve,
        greenCurve=curve,
        blueCurve=curve,
        masterCurve=curve,
        dirty=tr.bool,
    )
    ACESParameters.serde_members(
        temperature=tr.float,
        tint=tr.float,
        use_white_balance_mode=tr.bool,
        hueShift=tr.float,
        saturation=tr.float,
        contrast=tr.float,
        mixerRedOutRedIn=tr.float,
        mixerRedOutGreenIn=tr.float,
        mixerRedOutBlueIn=tr.float,
        mixerGreenOutRedIn=tr.float,
        mixerGreenOutGreenIn=tr.float,
        mixerGreenOutBlueIn=tr.float,
        mixerBlueOutRedIn=tr.float,
        mixerBlueOutGreenIn=tr.float,
        mixerBlueOutBlueIn=tr.float,
        lift=tr.float4,
        gamma=tr.float4,
        gain=tr.float4,
        colorFilter=tr.float4,
        tone_mapping=tr.external_type("ToneMappingParameters"),
    )

    ACESParameters.init_member(
        {
            "hueVsHueCurve": "{ float2(0, 0.5f) }",
            "hueVsSatCurve": "{ float2(0, 0.5f) }",
            "satVsSatCurve": "{ float2(0, 0.5f) }",
            "lumVsSatCurve": "{ float2(0, 0.5f) }",
            "temperature": "6500",
            "mixerRedOutRedIn": "100",
            "mixerGreenOutGreenIn": "100",
            "mixerBlueOutBlueIn": "100",
            "lift": "1, 1, 1, 0",
            "gamma": "1, 1, 1, 0",
            "gain": "1, 1, 1, 0",
            "colorFilter": "1, 1, 1, 1",
            "dirty": "true",
        }
    )
    ExposureSettings = tr.struct("ExposureSettings")
    ExposureSettings.serde_members(
        use_auto_exposure=tr.bool,
        filtering=tr.float2,
        minLuminance=tr.float,
        maxLuminance=tr.float,
        globalExposure=tr.float,
        speedUp=tr.float,
        speedDown=tr.float,
    )

    ExposureSettings.init_member(
        {
            "use_auto_exposure": "true",
            "filtering": "1.0f, 95.0f",
            "minLuminance": "-9",
            "maxLuminance": "9",
            "globalExposure": "0.5",
            "speedUp": "16",
            "speedDown": "16",
        }
    )
    PathTracerSettings = tr.struct("rbc::PathTracerSettings")
    PathTracerSettings.serde_members(
        offline_spp=tr.uint,
        offline_origin_bounce=tr.uint,
        offline_indirect_bounce=tr.uint,
        resource_color_space=ResourceColorSpace,
    )
    PathTracerSettings.init_member(
        {
            "offline_spp": "2",
            "offline_origin_bounce": "2",
            "offline_indirect_bounce": "4",
        }
    )
    ToneMappingSettings = tr.struct("rbc::ToneMappingSettings")
    ToneMappingSettings.serde_members(lpm=LpmDispatchParameters, aces=ACESParameters)
    ToneMappingSettings.init_member(
        {
            "use_lpm": "true",
        }
    )
    DisplaySettings = tr.struct("rbc::DisplaySettings")
    DisplaySettings.serde_members(
        use_linear_sdr=tr.bool,
        use_hdr_display=tr.bool,
        use_hdr_10=tr.bool,
        gamma=tr.float,
        chromatic_aberration=tr.float,
    )
    DisplaySettings.init_member(
        {
            "use_linear_sdr": "true",
            "use_hdr_display": "false",
            "use_hdr_10": "false",
            "gamma": "2.2",
            "chromatic_aberration": "0.001",
        }
    )
    SkySettings = tr.struct("rbc::SkySettings")
    SkySettings.members(
        sky_atom=tr.external_type("SkyAtmosphere*"),
        dirty=tr.bool,
        force_sync=tr.bool,
    )
    SkySettings.serde_members(
        sky_angle=tr.float,
        sky_max_lum=tr.float,
        sky_color=tr.float3,
        sun_color=tr.float3,
        sun_dir=tr.float3,
        sun_intensity=tr.float,
        sun_angle=tr.float,
    )
    SkySettings.init_member(
        {
            "force_sync": "true",
            "sky_max_lum": "65535",
            "sky_color": "1, 1, 1",
            "sun_color": "1, 1, 1",
            "sun_dir": "0, -1, 0",
            "sun_angle": "0.5",
        }
    )

    include = """#include <luisa/runtime/image.h>
#include <luisa/runtime/buffer.h>
#include <rbc_core/utils/curve.h>
#include <rbc_render/procedural/sky_atmosphere.h>"""

    ut.codegen_to(header_path)(cpp_interface_gen, include)
    include = "#include <rbc_render/generated/pipeline_settings.hpp>"
    ut.codegen_to(cpp_path)(cpp_impl_gen, include)
