# global variables
from robocute.rbc_ext._C import lcapi_c as lcapi

current_context = None
device = None
saved_shader_count = 0


def get_global_device():
    global device
    return device
