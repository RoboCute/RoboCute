from type_register import struct, double3, string, ClassPtr
from codegen_util import codegen_pyd_module
from pathlib import Path


def test_codegen():
    MyStruct = struct("MyStruct")
    MyStruct.member("set_pos", pos=double3, idx=int)
    MyStruct.member("get_pos", pos=string).ret_type(string)
    MyStruct.member("set_pointer", ptr=ClassPtr, name=string)
    pyd_name = "test_py_codegen"
    file_name = "example_module"
    root_path = Path(__file__).parent.parent.parent / Path(
        "rbc/tests/test_py_codegen/generated"
    )

    # codegen
    codegen_pyd_module(pyd_name, file_name, "py_backend_impl", root_path)
    print(file_name + " generated.")
