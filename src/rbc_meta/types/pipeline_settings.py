from typing import Annotated
from rbc_meta.utils.reflect import reflect, serde_field, no_serde_field
from rbc_meta.utils.builtin import (
    uint,
    uint2,
    ulong,
    float2,
    float3,
    float4,
    float3x3,
    double,
)
from enum import Enum


# External type helper
class ExternalType:
    """Helper class for external C++ types"""

    def __init__(self, cpp_type_name: str):
        self._cpp_type_name = cpp_type_name
        self._reflected_ = True


# Define external types
Curve = ExternalType("Curve")

SkyAtmospherePtr = ExternalType("SkyAtmosphere*")
BufferFloatConstPtr = ExternalType("luisa::compute::Buffer<float> const*")
ImageFloatConstPtr = ExternalType("luisa::compute::Image<float> const*")
ImageFloat = ExternalType("luisa::compute::Image<float>")


@reflect(
    cpp_namespace="rbc",
    serde=True,
    module_name="rbc_render",
)
class ToneMappingParameters:
    hdr_display_multiplier: float
    hdr_paper_white: float

    _cpp_init = {
        "hdr_display_multiplier": "5.0f",
        "hdr_paper_white": "80.0f",
    }


# Enums
@reflect(cpp_namespace="rbc", module_name="rbc_render")
class LpmColorSpace(Enum):
    REC709 = 0
    P3 = 1
    REC2020 = 2
    Display = 3


@reflect(cpp_namespace="rbc", module_name="rbc_render")
class ResourceColorSpace(Enum):
    Rec709 = 0
    AdobeRGB = 1
    P3_D60 = 2
    P3_D65 = 3
    Rec2020 = 4


@reflect(cpp_namespace="rbc", module_name="rbc_render")
class LpmDisplayMode(Enum):
    LDR = 0
    HDR10_2084 = 1
    HDR10_SCRGB = 2
    FSHDR_2084 = 3
    FSHDR_SCRGB = 4


@reflect(cpp_namespace="rbc", module_name="rbc_render")
class NRD_CheckerboardMode(Enum):
    OFF = None
    BLACK = None
    WHITE = None
    MAX_NUM = None


@reflect(cpp_namespace="rbc", module_name="rbc_render")
class NRD_HitDistanceReconstructionMode(Enum):
    # Probabilistic split at primary hit is not used, hence hit distance is always valid (reconstruction is not needed)
    OFF = None
    # If hit distance is invalid due to probabilistic sampling, reconstruct using 3x3 neighbors.
    # Probability at primary hit must be clamped to [1/4; 3/4] range to guarantee a sample in this area
    AREA_3X3 = None
    # If hit distance is invalid due to probabilistic sampling, reconstruct using 5x5 neighbors.
    # Probability at primary hit must be clamped to [1/16; 15/16] range to guarantee a sample in this area
    AREA_5X5 = None
    MAX_NUM = None


# Structs
@reflect(
    cpp_namespace="rbc",
    serde=True,
    module_name="rbc_render",
)
class DistortionSettings:
    scale: float
    intensity: float
    intensity_multiplier: float2
    center: float2

    _cpp_init = {
        "scale": "1.0",
        "intensity": "0.0",
        "intensity_multiplier": "1, 1",
        "center": "0.0f, 0.0f",
    }


@reflect(
    cpp_namespace="rbc",
    serde=True,
    module_name="rbc_render",
)
class LpmDispatchParameters:
    shoulder: bool
    softGap: float
    hdrMax: float
    lpmExposure: float
    contrast: float
    shoulderContrast: float
    saturation: float3
    crosstalk: float3
    colorSpace: LpmColorSpace
    displayMode: LpmDisplayMode
    displayRedPrimary: float2
    displayGreenPrimary: float2
    displayBluePrimary: float2
    displayWhitePoint: float2
    displayMinLuminance: float
    displayMaxLuminance: float

    _cpp_init = {
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


@reflect(cpp_namespace="rbc", module_name="rbc_render")
class FrameSettings:
    to_rec2020_matrix: float3x3
    render_resolution: uint2
    display_resolution: uint2
    display_offset: uint2
    frame_index: ulong
    delta_time: double
    resource_color_space: ResourceColorSpace
    realtime_rendering: bool
    offline_capturing: bool
    reject_sampling: bool
    albedo_buffer: BufferFloatConstPtr
    normal_buffer: BufferFloatConstPtr
    radiance_buffer: BufferFloatConstPtr
    resolved_img: ImageFloat
    dst_img: ImageFloatConstPtr

    _cpp_init = {
        "resource_color_space": "ResourceColorSpace::Rec709",
        "realtime_rendering": "true",
    }


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class ACESParameters:
    # Non-serde members (members only, not serialized)
    hueVsHueCurve: Annotated[Curve, no_serde_field()]
    hueVsSatCurve: Annotated[Curve, no_serde_field()]
    satVsSatCurve: Annotated[Curve, no_serde_field()]
    lumVsSatCurve: Annotated[Curve, no_serde_field()]
    redCurve: Annotated[Curve, no_serde_field()]
    greenCurve: Annotated[Curve, no_serde_field()]
    blueCurve: Annotated[Curve, no_serde_field()]
    masterCurve: Annotated[Curve, no_serde_field()]
    dirty: Annotated[bool, no_serde_field()]

    # Serde members (serialized) - using Annotated with serde_field()
    temperature: Annotated[float, serde_field()]
    tint: Annotated[float, serde_field()]
    use_white_balance_mode: Annotated[bool, serde_field()]
    hueShift: Annotated[float, serde_field()]
    saturation: Annotated[float, serde_field()]
    contrast: Annotated[float, serde_field()]
    mixerRedOutRedIn: Annotated[float, serde_field()]
    mixerRedOutGreenIn: Annotated[float, serde_field()]
    mixerRedOutBlueIn: Annotated[float, serde_field()]
    mixerGreenOutRedIn: Annotated[float, serde_field()]
    mixerGreenOutGreenIn: Annotated[float, serde_field()]
    mixerGreenOutBlueIn: Annotated[float, serde_field()]
    mixerBlueOutRedIn: Annotated[float, serde_field()]
    mixerBlueOutGreenIn: Annotated[float, serde_field()]
    mixerBlueOutBlueIn: Annotated[float, serde_field()]
    lift: Annotated[float4, serde_field()]
    gamma: Annotated[float4, serde_field()]
    gain: Annotated[float4, serde_field()]
    colorFilter: Annotated[float4, serde_field()]
    tone_mapping: Annotated[ToneMappingParameters, serde_field()]

    _cpp_init = {
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


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class ExposureSettings:
    use_auto_exposure: bool
    filtering: float2
    minLuminance: float
    maxLuminance: float
    globalExposure: float
    speedUp: float
    speedDown: float

    _cpp_init = {
        "use_auto_exposure": "true",
        "filtering": "1.0f, 95.0f",
        "minLuminance": "-9",
        "maxLuminance": "9",
        "globalExposure": "0.5",
        "speedUp": "16",
        "speedDown": "16",
    }


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class PathTracerSettings:
    offline_spp: uint
    offline_origin_bounce: uint
    offline_indirect_bounce: uint
    resource_color_space: ResourceColorSpace

    _cpp_init = {
        "offline_spp": "1",
        "offline_origin_bounce": "2",
        "offline_indirect_bounce": "4",
    }


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class ToneMappingSettings:
    lpm: LpmDispatchParameters
    aces: ACESParameters


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class DisplaySettings:
    use_linear_sdr: bool
    use_hdr_display: bool
    use_hdr_10: bool
    gamma: float
    chromatic_aberration: float

    _cpp_init = {
        "use_linear_sdr": "true",
        "use_hdr_display": "false",
        "use_hdr_10": "false",
        "gamma": "2.2",
        "chromatic_aberration": "0.001",
    }


@reflect(cpp_namespace="rbc", serde=True, module_name="rbc_render")
class SkySettings:
    # Non-serde members (members only, not serialized)
    sky_atom: Annotated[SkyAtmospherePtr, no_serde_field()]
    dirty: Annotated[bool, no_serde_field()]
    force_sync: Annotated[bool, no_serde_field()]

    # Serde members (serialized) - using Annotated with serde_field()
    sky_angle: Annotated[float, serde_field()]
    sky_max_lum: Annotated[float, serde_field()]
    sky_color: Annotated[float3, serde_field()]
    sun_color: Annotated[float3, serde_field()]
    sun_dir: Annotated[float3, serde_field()]
    sun_intensity: Annotated[float, serde_field()]
    sun_angle: Annotated[float, serde_field()]

    _cpp_init = {
        "force_sync": "true",
        "sky_max_lum": "65535",
        "sky_color": "1, 1, 1",
        "sun_color": "1, 1, 1",
        "sun_dir": "0, -1, 0",
        "sun_angle": "0.5",
    }
