from rbc_ext._C import test_py_codegen as lcapi


# callback: (level: string, message: string) -> None
def set_log_callback(callback):
    lcapi.set_log_callback(callback)


try:
    shell = get_ipython().__class__.__name__
    if shell != "TerminalInteractiveShell":
        from datetime import datetime


        def _default_log_callback(level, message):
            now = datetime.now()
            match level:
                case "D":
                    color, level = 96, "debug"
                case "I":
                    color, level = 92, "info"
                case "W":
                    color, level = 93, "warning"
                case _:
                    color, level = 91, "error"
            print(f"[{now}] [luisa] [\033[{color}m{level}\033[00m] {message}")


        set_log_callback(_default_log_callback)
except NameError:
    pass

from . import globalvars
from .types import half, short, ushort, half2, short2, ushort2, half3, short3, ushort3, half4, short4, ushort4

from .func import func
from .mathtypes import *
from .array import array, ArrayType, SharedArrayType
from .struct import struct, StructType
from .buffer import buffer, Buffer, ByteBuffer, BufferType, ByteBufferType, IndirectDispatchBuffer
from .image2d import image2d, Image2D, Texture2DType
from .image3d import image3d, Image3D, Texture3DType
from rbc_ext._C.test_py_codegen import PixelStorage

from .hit import TriangleHit, CommittedHit, ProceduralHit
from .rayquery import RayQueryAllType, RayQueryAnyType, is_triangle, is_procedural, Ray
from .util import RandomSampler
from .meshformat import MeshFormat

from rbc_ext._C.test_py_codegen  import log_level_verbose, log_level_info, log_level_warning, log_level_error
from os.path import realpath
import platform
import sys
from os import environ
import inspect



def verbose(fmt: str, *args, **kwargs):
    lcapi.log_verbose(fmt.format(*args, **kwargs))


def info(fmt: str, *args, **kwargs):
    lcapi.log_info(fmt.format(*args, **kwargs))


def warning(fmt: str, *args, **kwargs):
    lcapi.log_warning(fmt.format(*args, **kwargs))


def error(fmt: str, *args, **kwargs):
    lcapi.log_error(fmt.format(*args, **kwargs))


def _get_source_location_log_suffix():
    frame = inspect.currentframe().f_back.f_back
    return f" [{inspect.getfile(frame)}:{inspect.getlineno(frame)}]"


def verbose_with_location(fmt: str, *args, **kwargs):
    lcapi.log_verbose(fmt.format(*args, **kwargs) + _get_source_location_log_suffix())


def info_with_location(fmt: str, *args, **kwargs):
    lcapi.log_info(fmt.format(*args, **kwargs) + _get_source_location_log_suffix())


def warning_with_location(fmt: str, *args, **kwargs):
    lcapi.log_warning(fmt.format(*args, **kwargs) + _get_source_location_log_suffix())


def error_with_location(fmt: str, *args, **kwargs):
    lcapi.log_error(fmt.format(*args, **kwargs) + _get_source_location_log_suffix())


def init():
    if globalvars.device is not None:
        return
    globalvars.device = lcapi.get_default_lc_device()


def del_device():
    if globalvars.device is not None:
        del globalvars.device


def synchronize(stream=None):
    if stream is None:
        stream = globalvars.device
    stream.synchronize()


def execute(stream=None):
    if stream is None:
        stream = globalvars.device
    stream.execute()
