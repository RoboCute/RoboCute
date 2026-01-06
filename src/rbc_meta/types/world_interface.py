from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import DataBuffer, ExternalType
from rbc_meta.utils.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr, GUID, double, double3, float4, double4x4
from enum import Enum


@reflect(cpp_namespace="rbc", module_name="world_interface", pybind=True)
class ResourceLoadStatus(Enum):
    Unloaded = 0
    Loading = 1
    Loaded = 2


class Component:
    pass


Component._pybind_type_ = True


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    create_instance=False,
)
class Object:
    def instance_id() -> ulong: ...
    def guid() -> GUID: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    inherit=Object,
    create_instance=False,
)
class Resource:
    def load_status() -> ResourceLoadStatus: ...
    def loading() -> bool: ...
    def loaded() -> bool: ...
    def path() -> str: ...
    def save_to_path() -> bool: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    inherit=Object
)
class Entity:
    def add_component(name: str) -> Component: ...
    def get_component(name: str) -> Component: ...
    def remove_component(name: str) -> bool: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    inherit=Object,
    create_instance=False,
)
class Component:
    def entity() -> Entity: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    inherit=Component,
    create_instance=False,
)
class TransformComponent():
    def position() -> double3: ...
    def scale() -> double3: ...
    def rotation() -> float4: ...
    def trs() -> double4x4: ...
    def trs_float() -> float4x4: ...
    def children_count() -> ulong: ...
    def remove_children(children: VoidPtr) -> bool: ...
    def set_pos(pos: double3, recursive: bool) -> None: ...
    def set_scale(scale: double3, recursive: bool) -> None: ...
    def set_rotation(rotation: float4, recursive: bool) -> None: ...
    def set_trs_matrix(trs: double4x4, recursive: bool) -> None: ...
    def set_trs(pos: double3, rotation: float4,
                scale: double3, recursive: bool) -> None: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    create_instance=False,
    inherit=Component
)
class LightComponent():
    def add_area_light(luminance: float3, visible: bool) -> None: ...
    def add_disk_light(luminance: float3, visible: bool) -> None: ...
    def add_point_light(luminance: float3, visible: bool) -> None: ...
    def add_spot_light(luminance: float3, angle_radians: float,
                       small_angle_radians: float, angle_atten_pow: float, visible: bool) -> None: ...

    def luminance() -> float3: ...
    def angle_radians() -> float: ...
    def small_angle_radians() -> float: ...
    def angle_atten_pow() -> float: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="world_interface",
    create_instance=False,
    inherit=Component
)
class RenderComponent():
    pass
