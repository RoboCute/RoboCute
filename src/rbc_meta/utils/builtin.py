# RBC Builtin CLass


from typing import NewType, TypeVar, Generic


# uint = NewType("uint", int)


class size_t:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "size_t"


class uint:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint32_t"


class u8:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint8_t"


class u16:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint16_t"


class u32:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint32_t"


class u64:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint64_t"


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


class GUID:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "GUID"


class DataBuffer:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::span<std::byte>"


class float4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4"


class float4x4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4x4"


class double2:
    _reflected_ = True
    _cpp_type_name = "luisa::double2"


class double:
    _reflected_ = True
    _cpp_type_name = "double"


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


K = TypeVar("K")
V = TypeVar("V")


class UnorderedMap(Generic[K, V]):
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::unordered_map"
    _is_container = True
    _py_type_name = "dict"
