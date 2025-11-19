import type_register as tr
from codegen_util import codegen_pyd_module
import codegen_util as ut
from codegen import *
from pathlib import Path


def pipeline_settings():

    LpmColorSpace = tr.enum(
        'rbc::LpmColorSpace',
        REC709=0,
        P3=1,
        REC2020=2,
        Display=3,
    )
    ResourceColorSpace = tr.enum(
        'rbc::ResourceColorSpace',
        Rec709=0,
        AdobeRGB=1,
        P3_D60=2,
        P3_D65=3,
        Rec2020=4,
    )
    LpmDisplayMode = tr.enum(
        'rbc::LpmDisplayMode',
        LDR=0,
        HDR10_2084=1,
        HDR10_SCRGB=2,
        FSHDR_2084=3,
        FSHDR_SCRGB=4
    )
    EToneMappingMode = tr.enum(
        'rbc::EToneMappingMode',
        UnrealAces=0,
        UnityAces=None,
        FilmicAces=None
    )
    NRD_CheckerboardMode = tr.enum(
        'rbc::NRD_CheckerboardMode',
        OFF=None,
        BLACK=None,
        WHITE=None,
        MAX_NUM=None
    )
    NRD_HitDistanceReconstructionMode = tr.enum(
        'rbc::NRD_HitDistanceReconstructionMode',
        # Probabilistic split at primary hit is not used, hence hit distance is always valid (reconstruction is not needed)
        OFF=None,

        # If hit distance is invalid due to probabilistic sampling, reconstruct using 3x3 neighbors.
        # Probability at primary hit must be clamped to [1/4; 3/4] range to guarantee a sample in this area
        AREA_3X3=None,

        # If hit distance is invalid due to probabilistic sampling, reconstruct using 5x5 neighbors.
        # Probability at primary hit must be clamped to [1/16; 15/16] range to guarantee a sample in this area
        AREA_5X5=None,

        MAX_NUM=None
    )
    DistortionSettings = tr.struct('rbc::DistortionSettings')
    DistortionSettings.serde_members(
        scale=tr.float,
        intensity=tr.float,
        intensity_multiplier=tr.float2,
        center=tr.float2,
    )
    LpmDispatchParameters = tr.struct("rbc::LpmDispatchParameters")
    LpmDispatchParameters.serde_members(
        shoulder=tr.bool,
        softGap=tr.float,
        hdrMax=tr.float,
        lpmExposure=tr.float,
        contrast=tr.float,
        shoulderContrast=tr.float,
        saturation=tr.float,
        crosstalk=tr.float,
        colorSpace=LpmColorSpace,
        displayMode=LpmDisplayMode,
        displayRedPrimary=tr.float2,
        displayGreenPrimary=tr.float2,
        displayBluePrimary=tr.float2,
        displayWhitePoint=tr.float2,
        displayMinLuminance=tr.float2,
        displayMaxLuminance=tr.float2
    )
    ToneMappingParameters = tr.struct("ToneMappingParameters")
    ToneMappingParameters.serde_members(
        tone_mode=EToneMappingMode,
        unreal_tone_slope=tr.float,
        unreal_tone_toe=tr.float,
        unreal_tone_black_clip=tr.float,
        unreal_tone_shoulder=tr.float,
        unreal_tone_white_clip=tr.float,
        filmic_tone_shoulder_strength=tr.float,
        filmic_tone_linear_strength=tr.float,
        filmic_tone_linear_angle=tr.float,
        filmic_tone_toe_strength=tr.float,
        filmic_tone_toe_numerator=tr.float,
        filmic_tone_toe_denominator=tr.float,
        hdr_display_multiplier=tr.float,
        hdr_paper_white=tr.float,
    )

    path = Path("rbc/runtime/include/rbc_render/generated/pipeline_settings.hpp")
    ut.codegen_to(path, cpp_interface_gen)


if __name__ == '__main__':
    pipeline_settings()
