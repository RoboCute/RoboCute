from typing import NewType, TypeVar, Generic


# uint = NewType("uint", int)


class int:
    __slot__ = {}


class uint:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "uint32_t"


class ubyte:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "int8_t"
    _py_type_name = "ctypes.ubyte"


class ulong:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "unsigned long"
    _py_type_name = "ctypes.ulong"


class GUID:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "GUID"


class float4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4"


class float4x4:
    _reflected_ = True
    _cpp_type_name = "luisa::float4x4"


class double2:
    _reflected_ = True
    _cpp_type_name = "luisa::double2"


class string:
    _reflected_ = True
    _cpp_type_name = "luisa::string"


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
