import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
from rbc_meta.utils.codegen import cpp_interface_gen, cpp_impl_gen
from pathlib import Path
from rbc_meta.resource_enums import *


def codegen_header(header_path: Path, cpp_path: Path):
    suffix = 'RBC_RUNTIME_API'
    MeshMeta = tr.struct('rbc::MeshMeta', suffix)
    MeshMeta.serde_members(
        vertex_count=tr.uint,
        normal=tr.bool,
        tangent=tr.bool,
        uv_count=tr.uint,
        submesh_offset=tr.external_type("luisa::vector<uint32_t>")
    )
    TextureMeta = tr.struct('rbc::TextureMeta', suffix)
    TextureMeta.serde_members(
        width=tr.uint,
        height=tr.uint,
        storage = PixelStorage,
        mip_level=tr.uint,
    )
    include = """#include<luisa/runtime/rhi/pixel.h>
"""
    ut.codegen_to(header_path)(cpp_interface_gen, include)
    include = "#include <rbc_runtime/generated/resource_meta.hpp>"
    ut.codegen_to(cpp_path)(cpp_impl_gen, include)