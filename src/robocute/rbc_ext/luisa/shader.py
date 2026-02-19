from robocute.rbc_ext._C import lcapi_c as lcapi
from .globalvars import get_global_device
from .types import to_lctype, dtype_of


class Shader:
    """
    A wrapper class for loaded shaders that can be dispatched on the device.
    Shaders are lazy-loaded during the first invoke call.
    """

    def __init__(self, shader_path, argtypes=None):
        """
        Initialize a Shader object.
        
        Args:
            shader_path: Path to the compiled shader file
            argtypes: Optional tuple of argument types for type checking
        """
        self.shader_path = shader_path
        self.argtypes = argtypes
        self._shader_handle = None
        self._argument_size = 0

    def _load(self, argtypes): # argtypes: list[Type]
        """Lazy load the shader from disk. Called during first invoke."""
        if self._shader_handle is not None:
            return
        
        device = get_global_device()
        if device is None:
            raise RuntimeError("Device not initialized. Call init() first.")
        
        # Load the shader using the device's load_shader method
        self._shader_handle = device.load_shader(self.shader_path, argtypes)
        
        # Get function info from the loaded shader
        # The shader handle provides access to the function for dispatch
        # Get argument size from the shader handle if available
        if hasattr(self._shader_handle, 'argument_size'):
            self._argument_size = self._shader_handle.argument_size()
        else:
            self._argument_size = 0

    def __del__(self):
        """Clean up the shader resource when the object is destroyed."""
        if self._shader_handle is not None:
            device = get_global_device()
            if device is not None:
                device.destroy_shader(self._shader_handle)
            self._shader_handle = None
    def invoke(self, *args, dispatch_size=None, stream=None, dispatch_buffer_offset=0, max_dispatch_size=(2**32-1)):
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
        
        if stream is None:
            stream = device
        
        # Get 3D dispatch size or check if it's a buffer for indirect dispatch
        if type(dispatch_size) is int:
            dispatch_size = (dispatch_size, 1, 1)
        elif (type(dispatch_size) == tuple or type(dispatch_size) == list) and (len(dispatch_size) in (1, 2, 3)):
            dispatch_size = (*dispatch_size, *[1] * (3 - len(dispatch_size)))
        # else: dispatch_size is treated as a buffer for indirect dispatch
        
        # Type checking if argtypes were provided
        if self.argtypes is not None:
            actual_argtypes = tuple(dtype_of(a) for a in args)
            if len(actual_argtypes) != len(self.argtypes):
                raise TypeError(
                    f"Expected {len(self.argtypes)} arguments, got {len(actual_argtypes)}"
                )
        
        # Create command encoder for dispatch
        # Note: argument_size may need to be computed from args if not available from shader
        arg_size = self._argument_size
        if arg_size == 0 and len(args) > 0:
            # Estimate argument size from args
            arg_size = sum(self._get_arg_size(a) for a in args)
        for a in args:
            lctype = to_lctype(dtype_of(a))
            argtypes.append(lctype)
        command = lcapi.ComputeDispatchCmdEncoder.create(
            arg_size, self._shader_handle, argtypes
        )
        
        # Encode arguments
        argtypes = []
        for a_idx in range(args):
            a = args[a_idx]
            lctype = argtypes[a_idx]
            if lctype.is_basic():
                command.encode_uniform(lcapi.to_bytes(a), lctype.size(), lctype.alignment())
            elif lctype.is_array() or lctype.is_structure():
                command.encode_uniform(a.to_bytes(), lctype.size(), lctype.alignment())
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
        # Lazy load the shader on first invoke
        self._load(argtypes)
        
        # Set dispatch size or dispatch buffer
        assert type(dispatch_size) in (tuple, list)
        command.set_dispatch_size(*dispatch_size)
        
        # Add command to stream and execute
        stream.add(command.build())

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
