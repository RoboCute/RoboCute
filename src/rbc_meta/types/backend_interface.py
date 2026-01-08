from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import DataBuffer, ExternalType
from rbc_meta.utils.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr, GUID
from rbc_meta.types.resource_enums import LCPixelStorage

MaterialsVector = ExternalType(
    "luisa::vector<rbc::RC<rbc::RCBase>>", False, "Vec<rbc::RC<rbc::RCBase>>"
)


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
)
class RBCContext:

    def init_world(
        self, meta_path: str, binary_path: str
    ) -> None: ...
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

    def reset_frame_index(self) -> None: ...

    def set_view_camera(
        self, pos: float3, roll: float, pitch: float, yaw: float
    ) -> None: ...

    def disable_view(self) -> None: ...

    # tick
    def tick(self) -> None: ...

    def should_close(self) -> bool: ...
