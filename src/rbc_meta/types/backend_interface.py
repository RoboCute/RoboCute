from rbc_meta.utils_next.reflect import reflect
from rbc_meta.utils_next.builtin import DataBuffer
from rbc_meta.utils_next.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr
from rbc_meta.types.resource_enums import LCPixelStorage


# External type helper
class ExternalType:
    """Helper class for external C++ types"""

    def __init__(
        self,
        cpp_type_name: str,
        is_trivial_type: bool = False,
        py_codegen_name: str = None,
    ):
        self._cpp_type_name = cpp_type_name
        if py_codegen_name:
            self._py_codegen_name = py_codegen_name
        else:
            self._py_codegen_name = cpp_type_name
        self._reflected_ = True
        self._is_trivial_type = is_trivial_type

    def cpp_type_name(self, py_interface: bool = False, is_view: bool = True):
        if py_interface:
            name = self._py_codegen_name
        else:
            name = self._cpp_type_name
        if not self._is_trivial_type and is_view:
            name += " const&"
        return name


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
    # frame
    def init_device(
        self, rhi_backend: str, program_path: str, shader_path: str
    ) -> None: ...

    # render
    def init_render(self) -> None: ...

    def load_skybox(self, path: str, size: uint2) -> None: ...

    def create_window(self, name: str, size: uint2, resizable: bool) -> None: ...

    # mesh
    def create_mesh(
        self,
        vertex_count: uint,
        contained_normal: bool,
        contained_tangent: bool,
        uv_count: uint,
        triangle_count: uint,
        offsets_uint32: DataBuffer,
    ) -> VoidPtr: ...

    def load_mesh(
        self,
        file_path: str,
        file_offset: ulong,
        vertex_count: uint,
        contained_normal: bool,
        contained_tangent: bool,
        uv_count: uint,
        triangle_count: uint,
        offsets_uint32: DataBuffer,
    ) -> VoidPtr: ...

    def get_mesh_data(self, handle: VoidPtr) -> DataBuffer: ...

    def update_mesh(self, handle: VoidPtr, only_vertex: bool) -> None: ...

    # light
    def add_area_light(
        self, matrix: float4x4, luminance: float3, visible: bool
    ) -> VoidPtr: ...

    def add_disk_light(
        self,
        center: float3,
        radius: float,
        luminance: float3,
        forward_dir: float3,
        visible: bool,
    ) -> VoidPtr: ...

    def add_point_light(
        self, center: float3, radius: float, luminance: float3, visible: bool
    ) -> VoidPtr: ...

    def add_spot_light(
        self,
        center: float3,
        radius: float,
        luminance: float3,
        forward_dir: float3,
        angle_radians: float,
        small_angle_radians: float,
        angle_atten_pow: float,
        visible: bool,
    ) -> VoidPtr: ...

    def update_area_light(
        self, light: VoidPtr, matrix: float4x4, luminance: float3, visible: bool
    ) -> None: ...

    def update_disk_light(
        self,
        light: VoidPtr,
        center: float3,
        radius: float,
        luminance: float3,
        forward_dir: float3,
        visible: bool,
    ) -> None: ...

    def update_point_light(
        self,
        light: VoidPtr,
        center: float3,
        radius: float,
        luminance: float3,
        visible: bool,
    ) -> None: ...

    def update_spot_light(
        self,
        light: VoidPtr,
        center: float3,
        radius: float,
        luminance: float3,
        forward_dir: float3,
        angle_radians: float,
        small_angle_radians: float,
        angle_atten_pow: float,
        visible: bool,
    ) -> None: ...

    # texture
    def create_texture(
        self,
        storage: LCPixelStorage,
        size: uint2,
        mip_level: uint,
    ) -> VoidPtr: ...

    def get_texture_data(self, handle: VoidPtr) -> DataBuffer: ...

    def update_texture(self, handle: VoidPtr) -> None: ...

    def load_texture(
        self,
        path: str,
        file_offset: ulong,
        storage: LCPixelStorage,
        size: uint2,
        mip_level: uint,
        is_virtual_texture: bool,
    ) -> VoidPtr: ...

    def texture_heap_idx(self, ptr: VoidPtr) -> uint: ...

    # material
    def create_pbr_material(self) -> VoidPtr: ...

    def update_material(self, mat_ptr: VoidPtr, json: str) -> None: ...

    def get_material_json(self, mat: VoidPtr) -> str: ...

    # object
    def create_object(
        self, matrix: float4x4, mesh: VoidPtr, materials: MaterialsVector
    ) -> VoidPtr: ...

    def update_object_pos(self, object_ptr: VoidPtr, matrix: float4x4) -> None: ...

    def update_object(
        self,
        object_ptr: VoidPtr,
        matrix: float4x4,
        mesh: VoidPtr,
        materials: MaterialsVector,
    ) -> None: ...

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
