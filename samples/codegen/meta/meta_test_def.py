from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.codegenx import codegen, CodeModule, CodegenRegistry
from rbc_meta.utils.builtin import uint, Vector
from enum import Enum

@reflect(cpp_namespace="rbc")
class DummyEnum(Enum):
    a: int = 1
    b: float = 2.0

@reflect(
    cpp_namespace="rbc",
    serde=True,
    cpp_prefix="RBC_RUNTIME_API",
)
class DummyMeta:
    vertex_count: uint
    normal: bool
    tangent: bool
    uv_count: uint
    submesh_offset: Vector[uint]


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,  # Base type can not initialize
)
class BaseType:
    def base_get_value() -> int: ...
    def base_set_value(value: int) -> None: ...


@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc"
)
class DerivedType(BaseType):
    pass
