from rbc_meta.utils_next.reflect import reflect
from enum import Enum


@reflect(cpp_namespace="rbc", module_name="runtime")
class LCPixelStorage(Enum):
    BYTE1 = 1
    BYTE2 = 2
    BYTE4 = 3
    SHORT1 = 4
    SHORT2 = 5
    SHORT4 = 6
    INT1 = 7
    INT2 = 8
    INT4 = 9
    HALF1 = 10
    HALF2 = 11
    HALF4 = 12
    FLOAT1 = 13
    FLOAT2 = 14
    FLOAT4 = 15
    R10G10B10A2 = 16
    R11G11B10 = 17
    BC1 = 18
    BC2 = 19
    BC3 = 20
    BC4 = 21
    BC5 = 22
    BC6 = 23
    BC7 = 24
    BC7_SRGB = 25
    BYTE4_SRGB = 26


@reflect(cpp_namespace="rbc", module_name="runtime")
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
