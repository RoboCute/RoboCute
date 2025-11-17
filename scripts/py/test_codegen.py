from type_register import *
from codegen import *
from codegen_util import *
from pathlib import Path
def test_codegen():
    MyStruct = struct("MyStruct")
    MyStruct.member("set_pos", pos=double3, idx=int)
    MyStruct.member("get_pos", pos=string).ret_type(string)
    MyStruct.member("set_pointer", ptr=ClassPtr, name=string)
    module_name = 'test_py_codegen'
    file_name = 'example_module'
    root_path = Path(__file__).parent.parent.parent / Path('rbc/tests/test_py_codegen/generated')

    # codegen
    codegen_to(root_path / f"{file_name}.h", cpp_interface_gen)
    codegen_to(root_path / f"{file_name}.cpp", nanobind_codegen, module_name, f'{module_name}_impl', f'#include "{file_name}.h"')
    codegen_to(root_path / "main.cpp", nanobind_entry_codegen, module_name)
    codegen_to(root_path / f"{file_name}.py", py_interface_gen, module_name)
    print(file_name + ' generated.')