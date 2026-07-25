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
    Curve,
    SkyAtmosphere,
    LCBuffer,
    LCBufferView,
    LCImage,
    Pointer,
    Const,
)
from enum import Enum


# External type helper


@reflect(
    cpp_namespace="rbc",
    serde=True,
)
class ToneMappingParameters:
    hdr_display_multiplier: float
    hdr_paper_white: float

    _cpp_init = {
        "hdr_display_multiplier": "5.0f",
        "hdr_paper_white": "80.0f",
    }


# Enums
@reflect(cpp_namespace="rbc")
class LpmColorSpace(Enum):
    REC709 = 0
    P3 = 1
    REC2020 = 2
    Display = 3
    
@reflect(cpp_namespace="rbc", pybind=True)
class AlphaCull(Enum):
    NoCull = 0
    CullSkybox = 1
    OnlySkybox = 2
    CullAll = 3
    
@reflect(cpp_namespace="rbc")
class GeometryType(Enum):
    NONE = 0
    Depth = 1 << 0   # float: Distance to camera
    Normal = 1 << 1  # packed float3: normal-xyz
    # packed uint4:  X: object id  Y: primitive id ZW: triangle bary-centric (float2)
    ObjectID = 1 << 2
    PrimID = 1 << 3
    Barycentric = 1 << 4

    Emission = 1 << 5  # packed float3: emission color (sampled from spectrum)
    Albedo = 1 << 6  # packed float3: albedo color (sampled from spectrum)


@reflect(cpp_namespace="rbc")
class ResourceColorSpace(Enum):
    Rec709 = 0
    AdobeRGB = 1
    P3_D60 = 2
    P3_D65 = 3
    Rec2020 = 4


@reflect(cpp_namespace="rbc", pybind=True)
class SpectrumAccumulationSpace(Enum):
    XYZ = 0
    AP0D65 = 1


@reflect(cpp_namespace="rbc")
class LpmDisplayMode(Enum):
    LDR = 0
    HDR10_2084 = 1
    HDR10_SCRGB = 2
    FSHDR_2084 = 3
    FSHDR_SCRGB = 4


@reflect(cpp_namespace="rbc")
class NRD_CheckerboardMode(Enum):
    OFF = None
    BLACK = None
    WHITE = None
    MAX_NUM = None


@reflect(cpp_namespace="rbc")
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
    serde=True
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
    serde=True
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


@reflect(cpp_namespace="rbc")
class FrameSettings:
    to_rec2020_matrix: float3x3
    render_resolution: uint2
    display_resolution: uint2
    display_offset: uint2
    frame_index: ulong
    resource_color_space: ResourceColorSpace
    realtime_rendering: bool
    offline_capturing: bool
    reject_sampling: bool
    albedo_buffer: Pointer[Const[LCBuffer[float]]]
    normal_buffer: Pointer[Const[LCBuffer[float]]]
    radiance_buffer: Pointer[Const[LCBuffer[float]]]
    pt_geometry_buffer:  LCBufferView[float]
    geometry_channel: GeometryType
    resolved_img: LCImage[float]
    dst_img: Pointer[Const[LCImage[float]]]
    id_img: Pointer[Const[LCImage[uint]]]

    _cpp_init = {
        "resource_color_space": "ResourceColorSpace::Rec709",
        "realtime_rendering": "true",
        "geometry_channel": "GeometryType::NONE"
    }


@reflect(cpp_namespace="rbc", serde=True)
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


@reflect(cpp_namespace="rbc", serde=True)
class ExposureSettings:
    use_auto_exposure: bool
    filtering: float2
    minLuminance: float
    maxLuminance: float
    globalExposure: float

    _cpp_init = {
        "use_auto_exposure": "true",
        "filtering": "1.0f, 95.0f",
        "minLuminance": "-9",
        "maxLuminance": "9",
        "globalExposure": "0.5"
    }


@reflect(cpp_namespace="rbc", serde=True)
class PathTracerSettings:
    offline_spp: uint
    offline_origin_bounce: uint
    offline_indirect_bounce: uint
    probe_initial_medium: bool
    resource_color_space: ResourceColorSpace
    spectrum_accumulation_space: SpectrumAccumulationSpace
    denoise: bool
    # AO mode
    enable_ao_mode: bool
    # AO use cosine hemisphere sample instead of uniform sample
    ao_use_cosine_sample: bool
    # calculate 4 channel
    ao_max_radius: float4 
    ao_atten_pow: float4 

    _cpp_init = {
        "offline_spp": "1",
        "offline_origin_bounce": "2",
        "offline_indirect_bounce": "4",
        "probe_initial_medium": "true",
        "spectrum_accumulation_space": "SpectrumAccumulationSpace::AP0D65",
        "denoise": "true",
        "ao_max_radius": "1,1,1,1",
        "ao_atten_pow": "1,1,1,1",
        "ao_use_cosine_sample": "true"
    }


@reflect(cpp_namespace="rbc", serde=True)
class ToneMappingSettings:
    lpm: LpmDispatchParameters
    aces: ACESParameters


@reflect(cpp_namespace="rbc", serde=True)
class DisplaySettings:
    use_linear_sdr: bool
    use_hdr_display: bool
    use_hdr_10: bool
    alpha_cull: AlphaCull
    gamma: float
    chromatic_aberration: float

    _cpp_init = {
        "use_linear_sdr": "true",
        "use_hdr_display": "false",
        "use_hdr_10": "false",
        "gamma": "2.2",
        "chromatic_aberration": "0.001",
    }


@reflect(cpp_namespace="rbc", serde=True)
class SkySettings:
    # Non-serde members (members only, not serialized)
    sky_atom: Annotated[Pointer[SkyAtmosphere], no_serde_field()]
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

OUT_CLASSES = [
    ToneMappingParameters,
    LpmColorSpace,
    GeometryType,
    ResourceColorSpace,
    SpectrumAccumulationSpace,
    LpmDisplayMode,
    NRD_CheckerboardMode,
    NRD_HitDistanceReconstructionMode,
    DistortionSettings,
    LpmDispatchParameters,
    FrameSettings,
    ACESParameters,
    ExposureSettings,
    PathTracerSettings,
    ToneMappingSettings,
    DisplaySettings,
    SkySettings,
    AlphaCull,
]

__all__ = [
    "OUT_CLASSES"
]
