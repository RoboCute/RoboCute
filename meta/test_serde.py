import rbc_meta.type_register as tr
import rbc_meta.codegen as codegen
import rbc_meta.codegen_util as ut
from pathlib import Path


def test_serde():
    MyStruct = tr.struct("MyStruct")
    MyEnum = tr.enum("MyEnum", On=1, Off=None)
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
    path = (
        Path(__file__).parent.parent.parent
        / "rbc/tests/test_serde/generated/generated.hpp"
    )
    ut.codegen_to(path)(codegen.cpp_interface_gen)


if __name__ == "__main__":
    test_serde()
