import sys
from pathlib import Path
import os
import ctypes
dll_path = 'build/windows/x64/debug'
# rbc dll path
os.environ['RBC_RUNTIME_DIR'] = dll_path
# pyd path
sys.path.append(dll_path)
sys.path.append("scripts/py")
sys.path.append("scripts/py/generated")

from example_module import *
import test_py_codegen as tt

new_struct = MyStruct()
my_struct = MyStruct()
my_struct.set_pos(double3(444, 555.0, 666), 114514)
ptr = ctypes.c_void_p(0)
s = my_struct.get_pos("114_514")
print("result from python " + s)
my_struct.set_pointer(new_struct, "new_struct")
my_struct.set_pointer(my_struct, "my_struct")

my_guid = tt.GUID.new()
print("guid from python: " + my_guid.to_string())
my_enum = tt.MyEnum.enum_value_2
my_struct.set_guid(my_guid)
my_struct.set_enum(my_enum)
