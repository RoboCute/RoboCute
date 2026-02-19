from robocute.rbc_ext._C import lcapi_c as lcapi
from .globalvars import get_global_device
from .types import to_lctype, dtype_of


class Shader:
    """
    A wrapper class for loaded shaders that can be dispatched on the device.
    Shaders are lazy-loaded during the first invoke call.
    """

    def __init__(self, shader_path):
        """
        Initialize a Shader object.

        Args:
            shader_path: Path to the compiled shader file
            argtypes: Optional tuple of argument types for type checking
        """
        self.shader_path = shader_path
        self._argtypes = None
        self._shader_handle = None
        self._argument_size = 0

    def _load(self, argtypes):
        """Lazy load the shader from disk. Called during first invoke."""
        if self._shader_handle is not None:
            self._argtypes.check_same(argtypes)
            return
        self._argtypes = argtypes
        device = get_global_device()
        if device is None:
            raise RuntimeError("Device not initialized. Call init() first.")

        # Load the shader using the device's load_shader method
        self._shader_handle = device.load_shader(self.shader_path, argtypes)

        # Get function info from the loaded shader
        # The shader handle provides access to the function for dispatch
        # Get argument size from the shader handle if available
        self._argument_size = argtypes.size()

    def __del__(self):
        """Clean up the shader resource when the object is destroyed."""
        if self._shader_handle is not None:
            device = get_global_device()
            if device is not None:
                device.destroy_shader(self._shader_handle)
            self._shader_handle = None

    def __call__(self, *args, dispatch_size=None):
        assert dispatch_size
        """
        Invoke the shader with the given arguments.

        Args:
            *args: Arguments to pass to the shader
            dispatch_size: Tuple of (x, y, z) or int for 1D dispatch size, or a buffer for indirect dispatch
            stream: Stream to dispatch on (defaults to global device)
            dispatch_buffer_offset: Offset for indirect dispatch buffer
            max_dispatch_size: Maximum dispatch size for indirect dispatch
        """

        device = get_global_device()
        if device is None:
            raise RuntimeError("Device not initialized. Call init() first.")

        # Get 3D dispatch size or check if it's a buffer for indirect dispatch
        if type(dispatch_size) is int:
            dispatch_size = (dispatch_size, 1, 1)
        elif (type(dispatch_size) == tuple or type(dispatch_size) == list) and (len(dispatch_size) in (1, 2, 3)):
            dispatch_size = (*dispatch_size, *[1] * (3 - len(dispatch_size)))

        # else: dispatch_size is treated as a buffer for indirect dispatch

        # Build argtypes list from args
        argtypes = lcapi.ArgTypes()
        for a in args:
            lctype = to_lctype(dtype_of(a))
            argtypes.append(lctype)

        # Lazy load the shader on first invoke (passes argtypes for validation)
        self._load(argtypes)

        # Create command encoder for dispatch
        # Note: argument_size may need to be computed from args if not available from shader
        arg_size = self._argument_size
        if arg_size == 0 and len(args) > 0:
            # Estimate argument size from args
            arg_size = sum(self._get_arg_size(a) for a in args)

        command = lcapi.ComputeDispatchCmdEncoder.create(
            arg_size, self._shader_handle, argtypes
        )

        # Encode arguments
        for a_idx in range(len(args)):
            a = args[a_idx]
            lctype = argtypes.get(a_idx)
            if lctype.is_basic():
                command.encode_uniform(lcapi.to_bytes(
                    a), lctype.size(), lctype.alignment())
            elif lctype.is_array() or lctype.is_structure():
                command.encode_uniform(
                    a.to_bytes(), lctype.size(), lctype.alignment())
            elif lctype.is_buffer() or lctype.is_custom_buffer():
                command.encode_buffer(a.handle, 0, a.bytesize)
            elif lctype.is_texture():
                command.encode_texture(a.handle, 0)
            elif lctype.is_bindless_array():
                command.encode_bindless_array(a.handle)
            elif lctype.is_accel():
                command.encode_accel(a.handle)
            else:
                raise TypeError(f"Unsupported argument type: {type(a)}")

        # Set dispatch size or dispatch buffer
        assert type(dispatch_size) in (tuple, list)
        command.set_dispatch_size(*dispatch_size)

        # Add command to stream and execute
        device.add(command.build())

    def _get_arg_size(self, arg):
        """Get the size of an argument in bytes for encoding."""
        lctype = to_lctype(dtype_of(arg))
        if lctype.is_basic():
            return lctype.size()
        elif lctype.is_array() or lctype.is_structure():
            return lctype.size()
        elif lctype.is_buffer() or lctype.is_custom_buffer():
            return 8  # Handle size
        elif lctype.is_texture():
            return 8  # Handle size
        elif lctype.is_bindless_array():
            return 8  # Handle size
        elif lctype.is_accel():
            return 8  # Handle size
        return 0
