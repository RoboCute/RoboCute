import type_register as tr
from codegen_util import codegen_pyd_module
from pathlib import Path


def test_codegen():
    MyStruct = tr.struct("MyStruct")
    MyStruct.method("set_pos", pos=tr.double3, idx=tr.int)
    MyStruct.method("get_pos", pos=tr.string).ret_type(tr.string)
    MyStruct.method("set_pointer", ptr=tr.ClassPtr(MyStruct), name=tr.string)
    pyd_name = "test_py_codegen"
    file_name = "example_module"
    cpp_root_path = Path(__file__).parent.parent.parent / Path(
        "rbc/tests/test_py_codegen/generated"
    )
    py_root_path =  Path(__file__).parent / 'generated'
    
    # codegen
    codegen_pyd_module(pyd_name, file_name, "py_backend_impl", cpp_root_path, py_root_path)
    print(file_name + " generated.")
