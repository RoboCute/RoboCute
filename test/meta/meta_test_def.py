from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.codegenx import codegen, CodeModule, CodegenResitry
from rbc_meta.utils.builtin import uint, Vector


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
