from robocute.rbc_ext._C import lcapi_c as lcapi
from . import globalvars
from .globalvars import get_global_device
from .types import dtype_of, length_of, element_of, vector, to_lctype
from functools import cache
from .mathtypes import *
from .types import uint, uint2


class Image2D:
    def __init__(
        self, width, height, channel, dtype, mip=1, storage=None, external_memory=None
    ):
        if width == 0 or height == 0:
            raise Exception("Image2D size must be non-zero")
        if not dtype in {int, uint, float}:
            raise Exception("Image2D only supports int / uint / float")
        if not channel in (1, 2, 4):
            raise Exception("Image2D can only have 1/2/4 channels")
        self.width = width
        self.height = height
        self.channel = channel
        self.dtype = dtype
        self.mip = mip
        self.vectype = (
            dtype if channel == 1 else getattr(lcapi, dtype.__name__ + str(channel))
        )
        # default storage type: max precision
        self.storage = (
            storage
            if storage is not None
            else getattr(lcapi.PixelStorage, dtype.__name__.upper() + str(channel))
        )
        self.format = getattr(lcapi, "pixel_storage_to_format_" + dtype.__name__)(
            self.storage
        )

        self.bytesize = lcapi.pixel_storage_size(self.storage, width, height, 1)
        self.texture2DType = Texture2DType(dtype, channel)
        # instantiate texture on device
        if external_memory is not None:
            info = external_memory
        else:
            info = get_global_device().create_texture(
                self.format, 2, width, height, 1, mip
            )
        self.handle = info.handle()
        self.native_handle = info.native_handle()

    @staticmethod
    def import_native(dtype, info):
        if info.handle() == 18446744073709551615:
            return None
        # luisa.init()
        assert get_global_device() is not None
        return Image2D(
            info.width(),
            info.height(),
            info.channel(),
            dtype,
            info.mipmap_levels(),
            info.storage(),
            info,
        )

    def __del__(self):
        if self.handle is not None:
            device = get_global_device()
            if device is not None:
                device.destroy_texture(self.handle)

    @staticmethod
    def image2d(arr):
        if type(arr).__name__ == "ndarray":
            return Image2D.from_array(arr)
        else:
            raise TypeError(f"Image2D from unrecognized type: {type(arr)}")

    @staticmethod
    def empty(width, height, channel, dtype, storage=None):
        return Image2D(width, height, channel, dtype, 1, storage)

    @staticmethod
    def from_array(arr):  # arr: numpy array
        # TODO deduce dtype & storage
        assert (
            len(arr.shape) == 3
            and arr.shape[0] > 0
            and arr.shape[1] > 0
            and arr.shape[2] in (1, 2, 4)
        )
        tex = Image2D.empty(
            arr.shape[0], arr.shape[1], arr.shape[2], dtype_of(arr[0][0][0].item())
        )
        tex.copy_from_array(arr)
        return tex

    # use manually load temporarily

    @staticmethod
    def from_hdr_image(path: str):
        # load 32-bit 4-channel image from file
        import numpy as np

        arr = lcapi.load_hdr_image(path)
        assert len(arr.shape) == 3 and arr.shape[2] == 4
        assert arr.dtype == np.float32
        # save as SRGB 8-bit texture
        tex = Image2D.empty(arr.shape[0], arr.shape[1], 4, float)
        tex.copy_from_array(arr)
        return tex

    @staticmethod
    def from_ldr_image(path: str):
        # load 8-bit 4-channel image from file
        import numpy as np

        arr = lcapi.load_ldr_image(path)
        assert len(arr.shape) == 3 and arr.shape[2] == 4
        assert arr.dtype == np.ubyte
        # save as SRGB 8-bit texture
        tex = Image2D.empty(arr.shape[0], arr.shape[1], 4, float, storage="byte")
        tex.copy_from_array(arr)
        return tex

    def to_image(self, path: str):
        import numpy as np

        if self.format == lcapi.PixelFormat.RGBA32F:
            arr = np.empty([self.width, self.height, 4], dtype=np.float32)
            self.copy_to(arr)
            lcapi.save_hdr_image(path, arr, self.width, self.height)
            del arr
        elif self.format == lcapi.PixelFormat.RGBA8UNorm:
            arr = np.empty([self.width, self.height, 4], dtype=np.ubyte)
            self.copy_to(arr)
            lcapi.save_ldr_image(path, arr, self.width, self.height)
            del arr
        else:
            raise "Illegal export image format!"

    def copy_to_tex(self, tex, sync=False, stream=None):
        if stream is None:
            stream = globalvars.device
        assert (
            self.storage == tex.storage
            and self.width == tex.width
            and self.height == tex.height
        )
        cpcmd = lcapi.TextureCopyCommand.create(
            self.storage,
            self.handle,
            tex.handle,
            0,
            0,
            lcapi.uint3(self.width, self.height, 1),
        )
        stream.add(cpcmd)
        if sync:
            stream.synchronize()

    def copy_from_tex(self, tex, sync=False, stream=None):
        if stream is None:
            stream = globalvars.device
        assert (
            self.storage == tex.storage
            and self.width == tex.width
            and self.height == tex.height
        )
        cpcmd = lcapi.TextureCopyCommand.create(
            self.storage,
            tex.handle,
            self.handle,
            0,
            0,
            lcapi.uint3(self.width, self.height, 1),
        )
        stream.add(cpcmd)
        if sync:
            stream.synchronize()

    def copy_from(self, arr, sync=False, stream=None):
        if type(arr).__name__ == "ndarray":
            self.copy_from_array(arr, sync, stream)
        else:
            self.copy_from_tex(arr, sync, stream)

    def copy_from_array(self, arr, sync=False, stream=None):  # arr: numpy array
        if stream is None:
            stream = globalvars.device
        assert arr.size * arr.itemsize == self.bytesize
        ulcmd = lcapi.TextureUploadCommand.create(
            self.handle, self.storage, 0, lcapi.uint3(self.width, self.height, 1), arr
        )
        stream.add(ulcmd)
        stream.add_upload_buffer(arr)
        if sync:
            stream.synchronize()

    def copy_to(self, arr, sync=True, stream=None):  # arr: numpy array
        if stream is None:
            stream = globalvars.device
        assert arr.size * arr.itemsize == self.bytesize
        dlcmd = lcapi.TextureDownloadCommand.create(
            self.handle, self.storage, 0, lcapi.uint3(self.width, self.height, 1), arr
        )
        stream.add(dlcmd)
        # stream.add_readback_buffer(arr)
        if sync:
            stream.synchronize()


image2d = Image2D.image2d


class Texture2DType:
    def __init__(self, dtype, channel):
        self.dtype = dtype
        self.channel = channel
        self.vectype = (
            dtype if channel == 1 else getattr(lcapi, dtype.__name__ + str(channel))
        )

    def __eq__(self, other):
        return (
            type(other) is Texture2DType
            and self.dtype == other.dtype
            and self.channel == other.channel
        )

    def __hash__(self):
        return hash(self.dtype) ^ hash(self.channel) ^ 127858794396757894
