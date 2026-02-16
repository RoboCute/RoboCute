from robocute.rbc_ext._C import lcapi_c as lcapi
from . import globalvars
from .globalvars import get_global_device
from .types import length_of, element_of, vector, to_lctype
from functools import cache
from .mathtypes import *
from .types import uint, uint3


class Image3D:
    def __init__(
        self,
        width,
        height,
        volume,
        channel,
        dtype,
        mip=1,
        storage=None,
        external_memory=None,
    ):
        if width == 0 or height == 0 or volume == 0:
            raise Exception("Image3D size must be non-zero")
        if not dtype in {int, uint, float}:
            raise Exception("Image3D only supports int / uint / float")
        if not channel in (1, 3, 4):
            raise Exception("Image3D can only have 1/2/4 channels")
        self.width = width
        self.height = height
        self.channel = channel
        self.dtype = dtype
        self.mip = mip
        self.volume = volume
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

        self.bytesize = lcapi.pixel_storage_size(self.storage, width, height, volume)
        self.texture3DType = Texture3DType(dtype, channel)
        # instantiate texture on device
        if external_memory is not None:
            info = external_memory
        else:
            info = get_global_device().create_texture(
                self.format, 3, width, height, volume, mip
            )
        self.handle = info.handle()
        self.native_handle = info.native_handle()

    @staticmethod
    def import_native(dtype, info):
        if info.handle() == 18446744073709551615:
            return None
        assert get_global_device() is not None
        return Image3D(
            info.width(),
            info.height(),
            info.depth(),
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
    def image3d(arr):
        if type(arr).__name__ == "ndarray":
            return Image3D.from_array(arr)
        else:
            raise TypeError(f"Image3D from unrecognized type: {type(arr)}")

    @staticmethod
    def empty(width, height, channel, dtype, storage=None):
        return Image3D(width, height, channel, dtype, 1, storage)

    def copy_to_tex(self, tex, sync=False, stream=None):
        if stream is None:
            stream = globalvars.device
        assert (
            self.storage == tex.storage
            and self.width == tex.width
            and self.volume == tex.volume
            and self.height == tex.height
        )
        cpcmd = lcapi.TextureCopyCommand.create(
            self.storage,
            self.handle,
            tex.handle,
            0,
            0,
            lcapi.uint3(self.width, self.height, self.volume),
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
            and self.volume == tex.volume
            and self.height == tex.height
        )
        cpcmd = lcapi.TextureCopyCommand.create(
            self.storage,
            tex.handle,
            self.handle,
            0,
            0,
            lcapi.uint3(self.width, self.height, self.volume),
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
            self.handle,
            self.storage,
            0,
            lcapi.uint3(self.width, self.height, self.volume),
            arr,
        )
        stream.add(ulcmd)
        if sync:
            stream.synchronize()

    def copy_to(self, arr, sync=True, stream=None):  # arr: numpy array
        if stream is None:
            stream = globalvars.device
        assert arr.size * arr.itemsize == self.bytesize
        dlcmd = lcapi.TextureDownloadCommand.create(
            self.handle,
            self.storage,
            0,
            lcapi.uint3(self.width, self.height, self.volume),
            arr,
        )
        stream.add(dlcmd)
        # stream.add_readback_buffer(arr)
        if sync:
            stream.synchronize()


image3d = Image3D.image3d


class Texture3DType:
    def __init__(self, dtype, channel):
        self.dtype = dtype
        self.channel = channel
        self.vectype = (
            dtype if channel == 1 else getattr(lcapi, dtype.__name__ + str(channel))
        )

    def __eq__(self, other):
        return (
            type(other) is Texture3DType
            and self.dtype == other.dtype
            and self.channel == other.channel
        )

    def __hash__(self):
        return hash(self.dtype) ^ hash(self.channel) ^ 127858794396757894
