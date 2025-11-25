import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
import rbc_meta.utils.codegen as codegen
from pathlib import Path


def codegen_module(
    cpp_root_path: Path,
    py_root_path: Path,
):
    my_enum = tr.enum("MyEnum", enum_value_1=1, enum_value_2=None)

    MyStruct = tr.struct("MyStruct")
    MyStruct.method("set_pos", pos=tr.double3, idx=tr.int)
    MyStruct.method("get_pos", pos=tr.string).ret_type(tr.string)
    MyStruct.method("set_pointer", ptr=tr.ClassPtr(MyStruct), name=tr.string)
    MyStruct.method("set_guid", guid=tr.GUID)
    MyStruct.method("set_enum", enum_var=my_enum)

    pyd_name = "test_py_codegen"
    file_name = "example_module"

    # codegen
    ut.codegen_pyd_module(
        pyd_name, file_name, "py_backend_impl", cpp_root_path, py_root_path
    )
