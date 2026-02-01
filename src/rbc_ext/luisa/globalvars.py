# global variables
from rbc_ext._C import test_py_codegen as lcapi

current_context = None
device = None
saved_shader_count = 0

def get_global_device():
    global device
    return device
