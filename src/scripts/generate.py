from rbc_meta.utils.codegenx import CodegenResitry, codegen, CodeModule

from rbc_meta.types.resource_meta import MeshMeta, TextureMeta
from rbc_meta.types.resource_enums import LCPixelStorage, LCPixelFormat, SamplerFilter, SamplerAddress
from rbc_meta.types.pipeline_settings import (
    ToneMappingParameters,
    LpmColorSpace,
    GeometryType,
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
from rbc_meta.types.world_interface import (BasicDataType, ResourceLoadStatus, RendererGeometryType, BaseObjectType, Object, Entity, Component, TransformComponent, LightComponent, Resource, BasicData, TextureResource, MeshResource, BufferResource, MaterialResource, RenderComponent, RenderSettings, CameraComponent, DataComponent, EntitiesCollection, Scene, FileMeta, Project, TickStage, RBCContext)

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
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/runtime/"
    interface_header_file_ = (
        "rbc_plugin/generated/resource_meta_x.hpp"
    )
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/resource_meta_x.cpp"
    
    deps_ = [LuisaResourceModule]
    classes_ = [LCPixelStorage, LCPixelFormat, SamplerFilter, SamplerAddress, MeshMeta, TextureMeta]


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
        GeometryType,
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

@codegen
class WorldInterfacePyModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/tests/test_graphics"
    interface_header_file_ = "generated/world_x.h"

    header_files_ = ["res_creation_info.h", "rbc_plugin/generated/resource_meta.hpp", "rbc_world/resources/mesh.h"]


    classes_ = [
        BasicDataType, ResourceLoadStatus, RendererGeometryType, BaseObjectType, Object, Entity, Component, TransformComponent, LightComponent, Resource, BasicData, TextureResource, MeshResource, BufferResource, MaterialResource, RenderComponent, RenderSettings, CameraComponent, DataComponent, EntitiesCollection, Scene, FileMeta, Project, TickStage, RBCContext
    ]
    deps_ = [ResourceMetaModule]



def generate_registered():
    r = CodegenResitry()
    r.generate()
