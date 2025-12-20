from rbc_meta.utils.reflect import reflect
from enum import Enum


@reflect(cpp_namespace="rbc", module_name="runtime", pybind=True)
class LCPixelStorage(Enum):
    BYTE1 = 0
    BYTE2 = 1
    BYTE4 = 2
    SHORT1 = 3
    SHORT2 = 4
    SHORT4 = 5
    INT1 = 6
    INT2 = 7
    INT4 = 8
    HALF1 = 9
    HALF2 = 10
    HALF4 = 11
    FLOAT1 = 12
    FLOAT2 = 13
    FLOAT4 = 14
    R10G10B10A2 = 15
    R11G11B10 = 16
    BC1 = 17
    BC2 = 18
    BC3 = 19
    BC4 = 20
    BC5 = 21
    BC6 = 22
    BC7 = 23
    BC7_SRGB = 24
    BYTE4_SRGB = 25


@reflect(cpp_namespace="rbc", module_name="runtime", pybind=True)
class LCPixelFormat(Enum):
    R8SInt = 0
    R8UInt = 1
    R8UNorm = 2
    RG8SInt = 3
    RG8UInt = 4
    RG8UNorm = 5
    RGBA8SInt = 6
    RGBA8UInt = 7
    RGBA8UNorm = 8
    R16SInt = 9
    R16UInt = 10
    R16UNorm = 11
    RG16SInt = 12
    RG16UInt = 13
    RG16UNorm = 14
    RGBA16SInt = 15
    RGBA16UInt = 16
    RGBA16UNorm = 17
    R32SInt = 18
    R32UInt = 19
    R32UNorm = 20
    RG32SInt = 21
    RG32UInt = 22
    RG32UNorm = 23
    RGBA32SInt = 24
    RGBA32UInt = 25
    RGBA32UNorm = 26
    R16F = 27
    RG16F = 28
    RGBA16F = 29
    R32F = 30
    RG32F = 31
    RGBA32F = 32
    R10G10B10A2UInt = 33
    R10G10B10A2UNorm = 34
    R11G11B10F = 35
    BC1UNorm = 36
    BC2UNorm = 37
    BC3UNorm = 38
    BC4UNorm = 39
    BC5UNorm = 40
    BC6HUF16 = 41
    BC7UNorm = 42
    BC7SRGB = 43
    RGBA8SRGB = 44


@reflect(cpp_namespace="rbc", module_name="runtime")
class SamplerFilter(Enum):
    POINT = 0
    LINEAR_POINT = 1
    LINEAR_LINEAR = 2
    ANISOTROPIC = 3


@reflect(cpp_namespace="rbc", module_name="runtime")
class SamplerAddress(Enum):
    EDGE = 0
    REPEAT = 1
    MIRROR = 2
    ZERO = 3
