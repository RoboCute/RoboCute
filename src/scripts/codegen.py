from rbc_meta.utils.codegenx import codegen, CodeModule, CodegenResitry

from rbc_meta.types.pipeline_settings import (
    ToneMappingParameters,
    LpmColorSpace,
    ResourceColorSpace,
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
)
from rbc_meta.types.resource_meta import MeshMeta, TextureMeta


@codegen
class LuisaResourceModule(CodeModule):
    header_files_ = [
        "luisa/runtime/image.h",
        "luisa/runtime/buffer.h",
        "luisa/runtime/rhi/pixel.h",
    ]


@codegen
class RBCCoreModule(CodeModule):
    header_files_ = ["rbc_core/utils/curve.h"]


@codegen
class ResourceMetaModule(CodeModule):
    interface_header_file_ = (
        "rbc/runtime/include/rbc_plugin/generated/resource_meta_x.hpp"
    )
    cpp_impl_file_ = "rbc/runtime/src/generated/resource_meta_x.cpp"
    deps_ = [LuisaResourceModule]
    classes_ = [MeshMeta, TextureMeta]


@codegen
class PipelineSettingModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/render_plugin/"
    interface_header_file_ = "rbc_render/generated/pipeline_settings_x.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/pipeline_settings_x.cpp"
    header_files_ = ["rbc_render/procedural/sky_atmosphere.h"]
    deps_ = [LuisaResourceModule, RBCCoreModule]
    classes_ = [
        ToneMappingParameters,
        LpmColorSpace,
        ResourceColorSpace,
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
    ]


def generate_registered():
    r = CodegenResitry()
    r.generate()
