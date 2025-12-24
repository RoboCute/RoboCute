from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import DataBuffer, ExternalType
from rbc_meta.utils.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr, GUID
from rbc_meta.types.resource_enums import LCPixelStorage


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
    create_instance=False  # component can not be create
)
class TransformComponent:
    def guid() -> GUID: ...
    def instance_id() -> int: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
    create_instance=False  # component can not be create
)
class LightComponent:
    def guid() -> GUID: ...
    def instance_id() -> int: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
    create_instance=False  # component can not be create
)
class RenderComponent:
    def guid() -> GUID: ...
    def instance_id() -> int: ...
