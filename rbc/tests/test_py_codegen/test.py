import sys
from pathlib import Path
import os
# rbc dll path
os.environ['RBC_RUNTIME_DIR'] = 'build/windows/x64/debug'
# pyd path
sys.path.append('build/windows/x64/debug')
# py header path
sys.path.append(str(Path(__file__).parent / 'generated'))

from example_module import *

my_struct = MyStruct()
my_struct.set_pos(double3(444, 555.0, 666), 114514)
s = my_struct.get_pos('1919_810')
print("result from python " + s)