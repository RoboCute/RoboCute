import type_register as tr
import codegen
import codegen_util as ut
from pathlib import Path

def test_serde():
    MyStruct = tr.struct("MyStruct")
    MyStruct.members(
        a = tr.int,
        b = tr.double2,
        c = tr.float,
        dd = tr.string,
        ee = tr.vector(tr.int),
        ff = tr.unordered_map(tr.string, tr.float4)
    )
    path = Path(__file__).parent.parent.parent / "rbc/tests/test_serde/generated/generated.hpp"
    ut.codegen_to(path, codegen.cpp_interface_gen)

if __name__ == '__main__':
    test_serde()