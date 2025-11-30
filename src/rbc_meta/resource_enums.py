import rbc_meta.utils.type_register as tr
PixelStorage = tr.enum(
    'rbc::LCPixelStorage',
    BYTE1=None,
    BYTE2=None,
    BYTE4=None,

    SHORT1=None,
    SHORT2=None,
    SHORT4=None,

    INT1=None,
    INT2=None,
    INT4=None,

    HALF1=None,
    HALF2=None,
    HALF4=None,

    FLOAT1=None,
    FLOAT2=None,
    FLOAT4=None,

    R10G10B10A2=None,
    R11G11B10=None,

    BC1=None,
    BC2=None,
    BC3=None,
    BC4=None,
    BC5=None,
    BC6=None,
    BC7=None,
    BC7_SRGB=None,
    BYTE4_SRGB=None,
)
PixelFormat = tr.enum(
    'rbc::LCPixelFormat',
    R8SInt=None,
    R8UInt=None,
    R8UNorm=None,

    RG8SInt=None,
    RG8UInt=None,
    RG8UNorm=None,

    RGBA8SInt=None,
    RGBA8UInt=None,
    RGBA8UNorm=None,

    R16SInt=None,
    R16UInt=None,
    R16UNorm=None,

    RG16SInt=None,
    RG16UInt=None,
    RG16UNorm=None,

    RGBA16SInt=None,
    RGBA16UInt=None,
    RGBA16UNorm=None,

    R32SInt=None,
    R32UInt=None,

    RG32SInt=None,
    RG32UInt=None,

    RGBA32SInt=None,
    RGBA32UInt=None,

    R16F=None,
    RG16F=None,
    RGBA16F=None,

    R32F=None,
    RG32F=None,
    RGBA32F=None,

    R10G10B10A2UInt=None,
    R10G10B10A2UNorm=None,
    R11G11B10F=None,

    BC1UNorm=None,
    BC2UNorm=None,
    BC3UNorm=None,
    BC4UNorm=None,
    BC5UNorm=None,
    BC6HUF16=None,
    BC7UNorm=None,
    BC7SRGB=None,
    RGBA8SRGB=None,
)