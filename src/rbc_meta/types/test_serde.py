from rbc_meta.utils_next.reflect import reflect
from rbc_meta.utils_next.builtin import uint, GUID, Vector, float4x4
from typing import Enum


@reflect(cpp_namespace="rbc", serde=True, module_name="test_serde")
class MyEnum(Enum):
    On = 1
    Off = 2


@reflect(cpp_namespace="rbc", serde=True, module_name="test_serde")
class MyStruct:
    guid: GUID
    multi_dim_vec = Vector[Vector[int]]
    matrix: float4x4
    a: int
    # b=tr.double2,
    # c=tr.float,
    # dd=tr.string,
    # ee=tr.external_type("luisa::vector<int32_t>"),
    # ff=tr.unordered_map(tr.string, tr.float4),
    # vec_str=tr.external_type("luisa::vector<luisa::string>"),
    # test_enum=MyEnum,
