from rbc_meta.utils_next.reflect import reflect
from rbc_meta.utils_next.builtin import (
    uint,
    GUID,
    Vector,
    float4x4,
    double2,
    UnorderedMap,
    float4,
)
from enum import Enum


@reflect(cpp_namespace="rbc", serde=True, module_name="test_serde")
class MyEnum(Enum):
    On = 1
    Off = 2


@reflect(cpp_namespace="rbc", serde=True, module_name="test_serde")
class MyStruct:
    guid: GUID
    multi_dim_vec: Vector[Vector[int]]
    matrix: float4x4
    a: int
    b: double2
    c: float
    dd: str
    ee: Vector[int]
    ff: UnorderedMap[str, float4]
    vec_str: Vector[str]
    test_enum: MyEnum
