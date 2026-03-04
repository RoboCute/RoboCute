from rbc_meta.utils.codegenx import CodegenResitry, codegen, CodeModule

from rbc_meta.types.resource_meta import MeshMeta, TextureMeta
from rbc_meta.types.resource_enums import (
    LCPixelStorage,
    LCPixelFormat,
    SamplerFilter,
    SamplerAddress,
)
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
from rbc_meta.types.world_interface import (
    BasicDataType,
    ResourceLoadStatus,
    RendererGeometryType,
    BaseObjectType,
    Object,
    Entity,
    Component,
    TransformComponent,
    LightComponent,
    Resource,
    BasicData,
    TextureResource,
    MeshResource,
    BufferResource,
    MaterialResource,
    RenderComponent,
    RenderSettings,
    CameraComponent,
    DataComponent,
    DataComponentEventType,
    EntitiesCollection,
    Scene,
    FileMeta,
    Project,
    TickStage,
    SelectQuery,
    RBCContext,
    BuiltinKernels,
    SkeletonResource,
    SkinResource,
    AnimSequenceResource,
    AnimGraphResource,
    SkelMeshResource,
    AtmosphereComponent,
)


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


RESOURCE_CLASSES = [
    LCPixelStorage,
    LCPixelFormat,
    SamplerFilter,
    SamplerAddress,
    MeshMeta,
    TextureMeta,
]


@codegen
class ResourceMetaModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/runtime/"
    interface_header_file_ = "rbc_plugin/generated/resource_meta.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/resource_meta.cpp"

    deps_ = [LuisaResourceModule]
    classes_ = RESOURCE_CLASSES


PIPELINE_SETTING_CLASSES = [
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
class PipelineSettingModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/render_plugin/"
    interface_header_file_ = "rbc_render/generated/pipeline_settings.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/pipeline_settings.cpp"
    header_files_ = ["rbc_render/procedural/sky_atmosphere.h"]
    deps_ = [LuisaResourceModule, RBCCoreModule]
    classes_ = PIPELINE_SETTING_CLASSES


WORLD_INTERFACE_CLASSES = [
    BasicDataType,
    ResourceLoadStatus,
    RendererGeometryType,
    BaseObjectType,
    Object,
    Entity,
    Component,
    TransformComponent,
    LightComponent,
    Resource,
    BasicData,
    TextureResource,
    MeshResource,
    BufferResource,
    MaterialResource,
    RenderComponent,
    RenderSettings,
    CameraComponent,
    DataComponentEventType,
    DataComponent,
    EntitiesCollection,
    Scene,
    FileMeta,
    Project,
    TickStage,
    SelectQuery,
    RBCContext,
    BuiltinKernels,
    SkeletonResource,
    SkinResource,
    AnimSequenceResource,
    AnimGraphResource,
    SkelMeshResource,
    AtmosphereComponent,
]


@codegen
class WorldInterfaceModule(CodeModule):
    name_ = "world_interface"
    cpp_base_dir_ = "rbc/tests/test_graphics"
    header_files_ = [
        "res_creation_info.h",
        "rbc_plugin/generated/resource_meta.hpp",
        "rbc_world/resources/mesh.h",
    ]
    enable_cpp_interface_ = True
    interface_header_file_ = "generated/world.h"
    classes_ = WORLD_INTERFACE_CLASSES
    deps_ = [ResourceMetaModule]


EXT_CLASSES = []
EXT_CLASSES.extend(WORLD_INTERFACE_CLASSES)
EXT_CLASSES.extend(RESOURCE_CLASSES)


@codegen
class WorldInterfacePybindModule(CodeModule):
    name_ = "test_py_codegen"
    header_files_ = ["generated/world.h"]

    enable_pybind_cpp_def_ = True
    pybind_cpp_def_file_ = "rbc/tests/test_py_codegen/generated/world.cpp"
    enable_pybind_ = True
    pybind_py_file_ = "src/robocute/rbc_ext/generated/world.py"
    classes_ = EXT_CLASSES
    deps_ = [WorldInterfaceModule]


@codegen
class WorldInterfacePybindModuleX(CodeModule):
    name_ = "rbc_ext_c"
    header_files_ = ["generated/world.h"]
    enable_pybind_cpp_def_ = True
    pybind_cpp_def_file_ = "rbc/extensions/ext_c/src/generated/world.cpp"
    enable_pybind_ = True
    pybind_py_file_ = "src/robocute/rbc_ext/generated/world_v2.py"
    classes_ = EXT_CLASSES
    deps_ = [WorldInterfaceModule]


def generate_registered():
    r = CodegenResitry()
    r.generate()
