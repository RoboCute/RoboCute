# RBC Builtin CLass
from typing import NewType, TypeVar, Generic

# ===================================================
# ================== PRIMITIVE =====================
# ===================================================


class size_t:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "size_t"
    _py_type_name = "int"


class uint:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint32_t"
    _py_type_name = "int"


class u8:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint8_t"
    _py_type_name = "int"


class u16:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint16_t"
    _py_type_name = "int"


class u32:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint32_t"
    _py_type_name = "int"


class u64:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint64_t"
    _py_type_name = "int"


class ubyte:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "int8_t"
    _py_type_name = "int"


class ulong:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint64_t"
    _py_type_name = "int"


class long:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "int64_t"
    _py_type_name = "int"


class GUID:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "vstd::Guid"


class DataBuffer:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::span<std::byte>"
    _py_type_name = ""


class Callback:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::function<void()>"
    _py_type_name = ""


class float4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4"


class float4x4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4x4"


class double3:
    _reflected_ = True
    _cpp_type_name = "luisa::double3"


class double4:
    _reflected_ = True
    _cpp_type_name = "luisa::double4"


class double2:
    _reflected_ = True
    _cpp_type_name = "luisa::double2"


class double2x2:
    _reflected_ = True
    _cpp_type_name = "luisa::double2x2"


class double3x3:
    _reflected_ = True
    _cpp_type_name = "luisa::double3x3"


class double4x4:
    _reflected_ = True
    _cpp_type_name = "luisa::double4x4"


class double:
    _reflected_ = True
    _cpp_type_name = "double"
    _py_type_name = "float"


class float2:
    _reflected_ = True
    _cpp_type_name = "luisa::float2"


class float3:
    _reflected_ = True
    _cpp_type_name = "luisa::float3"


class float3x3:
    _reflected_ = True
    _cpp_type_name = "luisa::float3x3"


class uint2:
    _reflected_ = True
    _cpp_type_name = "luisa::uint2"


class uint3:
    _reflected_ = True
    _cpp_type_name = "luisa::uint3"


class uint4:
    _reflected_ = True
    _cpp_type_name = "luisa::uint4"


class string:
    _reflected_ = True
    _cpp_type_name = "luisa::string"


# ===================================================
# ================== STRUCT =====================
# ===================================================


class RBCStruct:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "rbc::RBCStruct"


class RCBase:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "rbc::RCBase"


# ===================================================
# ================== CONTAINERS =====================
# ===================================================


class VoidPtr:
    _reflected_ = True
    _cpp_type_name = "void*"
    _py_type_name = ""


T = TypeVar("T")


class Vector(Generic[T]):
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::vector"
    _is_container = True
    _py_type_name = "List"
    _pybind_cpp_name = "Vec"


class RC(Generic[T]):
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "rbc::RC"
    _is_container = True
    _py_type_name = "RC"


K = TypeVar("K")
V = TypeVar("V")


class UnorderedMap(Generic[K, V]):
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::unordered_map"
    _is_container = True
    _py_type_name = "dict"


class AsyncResource(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "rbc::AsyncResource"
    _is_container = True


# Special Class to generate V*
class Pointer(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _is_container = True


class Const(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _is_container = True


class Ref(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _is_container = True


class LCBuffer(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _is_container = True
    _cpp_type_name = "luisa::compute::Buffer"


class LCImage(Generic[V]):
    __slot__ = {}
    _reflected_ = True
    _is_container = True
    _cpp_type_name = "luisa::compute::Image"


class RenderMesh:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "rbc::RenderMesh"


# External type helper
class BuiltInMeshResource:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "world::MeshResource"


class Curve:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "Curve"


class SkyAtmosphere:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "SkyAtmosphere"
