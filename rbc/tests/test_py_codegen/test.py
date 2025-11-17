import sys
from pathlib import Path
import os
import ctypes
# rbc dll path
os.environ['RBC_RUNTIME_DIR'] = 'build/windows/x64/debug'
# pyd path
sys.path.append('build/windows/x64/debug')
# py header path
sys.path.append(str(Path(__file__).parent / 'generated'))

from example_module import *

new_struct = MyStruct()
my_struct = MyStruct()
my_struct.set_pos(double3(444, 555.0, 666), 114514)
ptr = ctypes.c_void_p(0)
s = my_struct.get_pos("114_514")
print("result from python " + s)
my_struct.set_pointer(new_struct, "new_struct")
my_struct.set_pointer(my_struct, "my_struct")
