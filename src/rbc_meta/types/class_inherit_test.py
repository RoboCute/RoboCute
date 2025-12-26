from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import DataBuffer, ExternalType
from rbc_meta.utils.builtin import uint, uint2, ulong, float3, float4x4, VoidPtr, GUID, double, double3, float4, double4x4



@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface",
    create_instance=False  # Base type can not initialize
)
class BaseType:
    def base_get_value() -> int: ...
    def base_set_value(value: int) -> None: ...
    
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    module_name="backend_interface"
)
class DeriveType(BaseType):
    pass
# DeriveType::base_get_value and DeriveType::base_set_value should be available