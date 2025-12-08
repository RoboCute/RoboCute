from rbc_meta.utils_next.reflect import reflect
from rbc_meta.utils_next.builtin import uint, Vector
from rbc_meta.types.resource_enums import LCPixelStorage


@reflect(
    cpp_namespace="rbc",
    serde=True,
    module_name="runtime",
    cpp_prefix="RBC_RUNTIME_API",
)
class MeshMeta:
    vertex_count: uint
    normal: bool
    tangent: bool
    uv_count: uint
    submesh_offset: Vector[uint]


@reflect(
    cpp_namespace="rbc",
    serde=True,
    module_name="runtime",
    cpp_prefix="RBC_RUNTIME_API",
)
class TextureMeta:
    width: uint
    height: uint
    storage: LCPixelStorage
    mip_level: uint
