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

new_struct = MyStruct()
my_struct = MyStruct()
my_struct.set_pos(double3(444, 555.0, 666), 114514)
ptr = ctypes.c_void_p(0)
s = my_struct.get_pos("114_514")
print("result from python " + s)
my_struct.set_pointer(new_struct, "new_struct")
my_struct.set_pointer(my_struct, "my_struct")
