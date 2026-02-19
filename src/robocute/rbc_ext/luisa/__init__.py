from robocute.rbc_ext._C import lcapi_c as lcapi


# callback: (level: string, message: string) -> None
def set_log_callback(callback):
    lcapi.set_log_callback(callback)

from . import globalvars
from .array import *
from .struct import *
from .types import (
    half,
    short,
    ushort,
    uint2,
    uint3,
    uint4,
    float2,
    float3,
    float4,
    double2,
    double3,
    double4,
    half2,
    short2,
    ushort2,
    half3,
    short3,
    ushort3,
    half4,
    short4,
    ushort4,
)

from .mathtypes import *
from .buffer import buffer, Buffer, ByteBuffer, BufferType, ByteBufferType
from .image2d import image2d, Image2D, Texture2DType
from .image3d import image3d, Image3D, Texture3DType
from .shader import Shader

from robocute.rbc_ext._C.lcapi_c import (
    log_level_verbose,
    log_level_info,
    log_level_warning,
    log_level_error,
)
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


capsule_vector = lcapi.capsule_vector


__all__ = [
    "init",
    "del_device",
    "execute",
    "uint2",
    "uint3",
    "uint4",
    "float2",
    "float3",
    "float4",
    "double2",
    "double3",
    "double4",
    "capsule_vector",
    "func",
    "set_block_size",
    "sync_block",
]
