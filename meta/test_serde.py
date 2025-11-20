import rbc_meta.type_register as tr
import rbc_meta.codegen as codegen
import rbc_meta.codegen_util as ut
from pathlib import Path


def codegen_header(header_path: Path):
    MyStruct = tr.struct("rbc::MyStruct")
    MyEnum = tr.enum("rbc::MyEnum", On=1, Off=None)
    MyStruct.serde_members(
        guid=tr.GUID,
        multi_dim_vec=tr.vector(tr.vector(tr.int)),
        a=tr.int,
        b=tr.double2,
        c=tr.float,
        dd=tr.string,
        ee=tr.vector(tr.int),
        ff=tr.unordered_map(tr.string, tr.float4),
        vec_str=tr.vector(tr.string),
        test_enum=MyEnum,
    )
    ut.codegen_to(header_path)(codegen.cpp_interface_gen)
