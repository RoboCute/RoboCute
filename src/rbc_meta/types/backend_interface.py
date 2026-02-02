from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr, GUID
from rbc_meta.types.resource_enums import LCPixelStorage
from enum import Enum


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
)
class TickStage(Enum):
    NONE = 0
    RasterPreview = 1
    PathTracingPreview = 2
    OffineCapturing = 3
    PresentOfflineResult = 4


class LCPYBuffer:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::compute::BufferCreationInfoInterop"
    _py_type_name = "luisa.Buffer"
    _ctor_begin = "'import_native(int,'"


class LCPYImage2D:
    __slot__ = {}
    _cpp_type_name = "luisa::compute::TextureCreationInfo"
    _py_type_name = "luisa.Image2D"
    _ctor_begin = "import_native(float,"


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
)
class RBCContext:
    def init_world(self, meta_path: str, binary_path: str) -> None: ...

    # frame

    def init_device(
        self, rhi_backend: str, program_path: str, shader_path: str
    ) -> None: ...

    # render
    def init_render(self) -> None: ...

    def create_window(self, name: str, size: uint2,
                      resizable: bool) -> None: ...

    # view
    def reset_view(self, resolution: uint2) -> None: ...

    def set_view_camera(
        self, pos: float3, roll: float, pitch: float, yaw: float
    ) -> None: ...

    def disable_view(self) -> None: ...

    # tick
    def tick(
        self,
        delta_time: float,
        resolution: uint2,
        frame_index: uint,
        tick_stage: TickStage,
        prepare_denoise: bool,
    ) -> None: ...

    def denoise(self) -> None: ...
    def save_display_image_to(self, path: str) -> None: ...
    def should_close(self) -> bool: ...
    def display_image(self) -> LCPYImage2D: ...
