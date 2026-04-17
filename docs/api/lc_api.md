# Luisa Compute Python API Documentation

This document describes the Python API for the Luisa Compute (lcapi) integration in RoboCute.

## Table of Contents

- [Core Module](#core-module)
- [Data Types](#data-types)
- [Math Types](#math-types)
- [Arrays](#arrays)
- [Buffers](#buffers)
- [Structures](#structures)
- [2D Images/Textures](#2d-imagestextures)
- [3D Images/Textures](#3d-imagestextures)
- [Shaders](#shaders)
- [Utilities](#utilities)

---

## Core Module

The core module provides initialization, logging, and device management functions.

### Functions

#### `init()`
Initialize the Luisa Compute device.
```python
def init()
```
If the device is already initialized, this function does nothing.

#### `del_device()`
Delete/release the Luisa Compute device.
```python
def del_device()
```

#### `synchronize(stream=None)`
Synchronize the device or a specific stream.
```python
def synchronize(stream=None)
```
- **Parameters:**
  - `stream`: Stream to synchronize (defaults to global device)

#### `execute(stream=None)`
Execute pending commands on the device or a specific stream.
```python
def execute(stream=None)
```
- **Parameters:**
  - `stream`: Stream to execute on (defaults to global device)

#### `set_log_callback(callback)`
Set a custom log callback function.
```python
def set_log_callback(callback)
```
- **Parameters:**
  - `callback`: Function with signature `(level: str, message: str) -> None`

### Logging Functions

#### `verbose(fmt, *args, **kwargs)`
Log a verbose message.
```python
def verbose(fmt: str, *args, **kwargs)
```

#### `info(fmt, *args, **kwargs)`
Log an info message.
```python
def info(fmt: str, *args, **kwargs)
```

#### `warning(fmt, *args, **kwargs)`
Log a warning message.
```python
def warning(fmt: str, *args, **kwargs)
```

#### `error(fmt, *args, **kwargs)`
Log an error message.
```python
def error(fmt: str, *args, **kwargs)
```

### Logging with Source Location

These functions append the source file and line number to the log message:

- `verbose_with_location(fmt, *args, **kwargs)`
- `info_with_location(fmt, *args, **kwargs)`
- `warning_with_location(fmt, *args, **kwargs)`
- `error_with_location(fmt, *args, **kwargs)`

### Log Level Constants

- `log_level_verbose`
- `log_level_info`
- `log_level_warning`
- `log_level_error`

### Capsule Vector

#### `capsule_vector`
A utility for creating capsule vectors (imported from C++ backend).

---

## Data Types

The `types` module defines scalar, vector, and matrix types for GPU computation.

### Scalar Types

Custom scalar type markers:

| Type | Description |
|------|-------------|
| `uint` | Unsigned 32-bit integer |
| `ushort` | Unsigned 16-bit integer |
| `half` | 16-bit floating point |
| `short` | Signed 16-bit integer |
| `long` | Signed 64-bit integer |
| `ulong` | Unsigned 64-bit integer |

### Vector Types

2-component vectors:
- `int2`, `uint2`, `bool2`, `float2`, `double2`
- `short2`, `half2`, `ushort2`
- `long2`, `ulong2`

3-component vectors:
- `int3`, `uint3`, `bool3`, `float3`, `double3`
- `short3`, `half3`, `ushort3`
- `long3`, `ulong3`

4-component vectors:
- `int4`, `uint4`, `bool4`, `float4`, `double4`
- `short4`, `half4`, `ushort4`
- `long4`, `ulong4`

### Matrix Types

- `float2x2` - 2x2 float matrix
- `float3x3` - 3x3 float matrix
- `float4x4` - 4x4 float matrix

### Type Categories

Sets for type checking:

```python
scalar_dtypes = {int, float, bool, uint, ushort, half, short, long, ulong}
vector_dtypes = {int2, float2, ...}  # All vector types
matrix_dtypes = {float2x2, float3x3, float4x4}
basic_dtypes = scalar_dtypes ∪ vector_dtypes ∪ matrix_dtypes
arithmetic_dtypes = {int, uint, float, ...}  # Numeric types only
integer_scalar_vector_dtypes = {int, int2, int3, int4, uint, uint2, ...}
```

### Type Utility Functions

#### `nameof(dtype)`
Get the name of a data type.
```python
def nameof(dtype) -> str
```

#### `vector(dtype, length)`
Create a vector type from scalar type and length.
```python
def vector(dtype, length)
```
- Example: `vector(float, 3)` returns `float3`

#### `vector16(dtype, length)`
Create a 16-bit vector type.
```python
def vector16(dtype, length)
```

#### `vector32(dtype, length)`
Create a 32-bit vector type.
```python
def vector32(dtype, length)
```

#### `length_of(dtype)`
Get the component count of a type.
```python
def length_of(dtype) -> int
```
- Returns 1 for scalars
- Returns 2, 3, or 4 for vectors

#### `element_of(dtype)`
Get the element type of a vector/matrix.
```python
def element_of(dtype)
```
- Example: `element_of(float3)` returns `float`

#### `is_bit16_types(dtype)`
Check if type is 16-bit.
```python
def is_bit16_types(dtype) -> bool
```

#### `is_bit64_types(dtype)`
Check if type is 64-bit.
```python
def is_bit64_types(dtype) -> bool
```

#### `dtype_of(val)`
Determine the data type of a value.
```python
def dtype_of(val)
```

#### `to_lctype(dtype)`
Convert Python dtype to Luisa Compute type.
```python
def to_lctype(dtype)
```

#### `from_lctype(lctype)`
Convert Luisa Compute type to Python dtype.
```python
def from_lctype(lctype)
```

#### `implicit_convertible(src, dst)`
Check if source type can be implicitly converted to destination type.
```python
def implicit_convertible(src, dst) -> bool
```

---

## Math Types

The `mathtypes` module provides convenient constructors for vector and matrix types.

### Vector Constructors

```python
make_int2(x, y) -> int2
make_bool2(x, y) -> bool2
make_float2(x, y) -> float2
make_int3(x, y, z) -> int3
make_bool3(x, y, z) -> bool3
make_float3(x, y, z) -> float3
make_int4(x, y, z, w) -> int4
make_bool4(x, y, z, w) -> bool4
make_float4(x, y, z, w) -> float4
```

### Matrix Constructors

```python
make_float2x2(...) -> float2x2
make_float3x3(...) -> float3x3
make_float4x4(...) -> float4x4
```

---

## Arrays

The `array` module provides fixed-size array types for GPU data.

### `Array`

A fixed-size array with homogeneous element types.

```python
class Array:
    def __init__(self, arr)
    def __len__(self) -> int
    def __getitem__(self, idx)
    def __setitem__(self, idx, value)
    def copy(self) -> Array
    def to_bytes(self) -> bytes
```

#### Constructor
```python
Array(arr)
```
- **Parameters:**
  - `arr`: List of values or another Array to copy

#### Methods

| Method | Description |
|--------|-------------|
| `copy()` | Create a copy of the array |
| `to_bytes()` | Serialize array to bytes |

### `ArrayType`

Type descriptor for arrays.

```python
class ArrayType:
    def __init__(self, size, dtype)
    def __call__(self, data) -> Array
```

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `size` | Number of elements |
| `dtype` | Element data type |
| `luisa_type` | Corresponding Luisa Compute type |
| `size_bytes` | Total size in bytes |

### `SharedArrayType`

Type descriptor for shared (GPU shared memory) arrays.

```python
class SharedArrayType:
    def __init__(self, size, dtype)
```

### Utility Functions

#### `array(arr)`
Create an Array from a list.
```python
def array(arr) -> Array
```

#### `deduce_array_type(arr)`
Infer the ArrayType from an array's contents.
```python
def deduce_array_type(arr) -> ArrayType
```

---

## Buffers

The `buffer` module provides GPU buffer management for structured data.

### `Buffer`

A GPU buffer for storing structured data.

```python
class Buffer:
    def __init__(self, size, dtype, external_memory=None, enable_interop=False)
```

#### Constructor
```python
Buffer(size, dtype, external_memory=None, enable_interop=False)
```
- **Parameters:**
  - `size`: Number of elements
  - `dtype`: Element data type
  - `external_memory`: Optional external memory handle
  - `enable_interop`: Enable CUDA/graphics interop

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `size` | Number of elements |
| `dtype` | Element data type |
| `stride` | Element size in bytes |
| `bytesize` | Total buffer size in bytes |
| `handle` | GPU buffer handle |
| `native_handle` | Native (CUDA/etc) handle |

#### Static Methods

| Method | Description |
|--------|-------------|
| `buffer(arr)` | Create buffer from ndarray or list |
| `from_list(arr)` | Create buffer from Python list |
| `from_array(arr)` | Create buffer from numpy array |
| `import_native(dtype, info)` | Import from native buffer info |
| `import_external_memory(addr, count, dtype)` | Import external memory |

#### Methods

| Method | Description |
|--------|-------------|
| `copy_from(arr, sync=False, stream=None)` | Copy data from array/list |
| `copy_from_list(arr, sync=False, stream=None)` | Copy from Python list |
| `copy_from_array(arr, sync=False, stream=None)` | Copy from numpy array |
| `copy_to(arr, sync=True, stream=None)` | Copy data to numpy array |
| `numpy(stream=None)` | Convert to numpy array |
| `to_list(stream=None)` | Convert to Python list |
| `interop_copy_from(cu_ptr, cu_stream, offset=0, size=None)` | CUDA interop copy from |
| `interop_copy_to(cu_ptr, cu_stream, offset=0, size=None)` | CUDA interop copy to |
| `info()` | Get buffer creation info |

#### DLPack Support

| Method | Description |
|--------|-------------|
| `__dlpack__()` | Export to DLPack |
| `__dlpack_device__()` | Get DLPack device info |
| `from_dlpack(arr)` | Import from DLPack (static) |

### `BufferType`

Type descriptor for buffers.

```python
class BufferType:
    def __init__(self, dtype)
```

### `ByteBuffer`

A raw byte buffer without type information.

```python
class ByteBuffer:
    def __init__(self, size)
```

#### Static Methods

| Method | Description |
|--------|-------------|
| `buffer(arr)` | Create from ndarray or list |
| `empty(size)` | Create empty buffer |
| `from_list(arr)` | Create from list |
| `from_array(arr)` | Create from numpy array |

### `ByteBufferType`

Type descriptor for byte buffers.

```python
class ByteBufferType:
    def __init__(self)
```

### Utility Functions

#### `from_bytes(dtype, packed)`
Deserialize bytes to typed value.
```python
def from_bytes(dtype, packed)
```

---

## Structures

The `struct` module provides struct types for GPU data.

### `Struct`

A structured data type with named fields.

```python
class Struct:
    def __init__(self, copy_source=None, alignment=1, **kwargs)
    def copy(self) -> Struct
    def to_bytes(self) -> bytes
```

#### Constructor
```python
Struct(copy_source=None, alignment=1, **kwargs)
```
- **Parameters:**
  - `copy_source`: Another Struct to copy from
  - `alignment`: Memory alignment requirement
  - `**kwargs`: Field name-value pairs

#### Methods

| Method | Description |
|--------|-------------|
| `copy()` | Create a copy |
| `to_bytes()` | Serialize to bytes |

#### Static Methods

| Method | Description |
|--------|-------------|
| `cast(dtype, value)` | Cast value to dtype |

### `StructType`

Type descriptor for structs.

```python
class StructType:
    def __init__(self, alignment=1, **kwargs)
    def __call__(self, **kwargs) -> Struct
    def add_method(func, name=None)
```

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `membertype` | List of field types |
| `idx_dict` | Field name to index mapping |
| `method_dict` | Field name to method mapping |
| `alignment` | Memory alignment |
| `luisa_type` | Luisa Compute type |
| `size_bytes` | Total size in bytes |

### `CustomType`

A custom/user-defined type.

```python
class CustomType:
    def __init__(self, name: str)
```

### Utility Functions

#### `struct(alignment=1, **kwargs)`
Create a Struct with fields.
```python
def struct(alignment=1, **kwargs) -> Struct
```

#### `deduce_struct_type(kwargs, alignment=1)`
Infer StructType from field values.
```python
def deduce_struct_type(kwargs, alignment=1) -> StructType
```

---

## 2D Images/Textures

The `image2d` module provides 2D texture/image support.

### `Image2D`

A 2D image/texture for GPU computation.

```python
class Image2D:
    def __init__(self, width, height, channel, dtype, mip=1, storage=None, external_memory=None)
```

#### Constructor
```python
Image2D(width, height, channel, dtype, mip=1, storage=None, external_memory=None)
```
- **Parameters:**
  - `width`: Image width in pixels
  - `height`: Image height in pixels
  - `channel`: Number of channels (1, 2, or 4)
  - `dtype`: Data type (int, uint, or float)
  - `mip`: Mipmap levels (default 1)
  - `storage`: Pixel storage format
  - `external_memory`: Optional external memory

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `width` | Image width |
| `height` | Image height |
| `channel` | Number of channels |
| `dtype` | Element data type |
| `mip` | Mipmap levels |
| `storage` | Pixel storage format |
| `format` | Pixel format |
| `bytesize` | Total size in bytes |
| `handle` | GPU texture handle |
| `native_handle` | Native handle |

#### Static Methods

| Method | Description |
|--------|-------------|
| `image2d(arr)` | Create from numpy array |
| `empty(width, height, channel, dtype, storage=None)` | Create empty image |
| `from_array(arr)` | Create from numpy array |
| `from_hdr_image(path)` | Load HDR image from file |
| `from_ldr_image(path)` | Load LDR image from file |
| `import_native(dtype, info)` | Import from native texture info |

#### Methods

| Method | Description |
|--------|-------------|
| `copy_from(arr, sync=False, stream=None)` | Copy data from array/texture |
| `copy_from_array(arr, sync=False, stream=None)` | Copy from numpy array |
| `copy_from_tex(tex, sync=False, stream=None)` | Copy from another texture |
| `copy_to(arr, sync=True, stream=None)` | Copy to numpy array |
| `copy_to_tex(tex, sync=False, stream=None)` | Copy to another texture |
| `to_image(path)` | Save to image file (HDR or LDR) |

### `Texture2DType`

Type descriptor for 2D textures.

```python
class Texture2DType:
    def __init__(self, dtype, channel)
```

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `dtype` | Element data type |
| `channel` | Number of channels |
| `vectype` | Vector type for pixels |
| `luisa_type` | Luisa Compute type |

---

## 3D Images/Textures

The `image3d` module provides 3D texture/volume support.

### `Image3D`

A 3D image/texture (volume) for GPU computation.

```python
class Image3D:
    def __init__(self, width, height, volume, channel, dtype, mip=1, storage=None, external_memory=None)
```

#### Constructor
```python
Image3D(width, height, volume, channel, dtype, mip=1, storage=None, external_memory=None)
```
- **Parameters:**
  - `width`: Width in voxels
  - `height`: Height in voxels
  - `volume`: Depth in voxels
  - `channel`: Number of channels (1, 3, or 4)
  - `dtype`: Data type (int, uint, or float)
  - `mip`: Mipmap levels (default 1)
  - `storage`: Pixel storage format
  - `external_memory`: Optional external memory

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `width` | Volume width |
| `height` | Volume height |
| `volume` | Volume depth |
| `channel` | Number of channels |
| `dtype` | Element data type |
| `mip` | Mipmap levels |
| `storage` | Pixel storage format |
| `format` | Pixel format |
| `bytesize` | Total size in bytes |
| `handle` | GPU texture handle |
| `native_handle` | Native handle |

#### Static Methods

| Method | Description |
|--------|-------------|
| `image3d(arr)` | Create from numpy array |
| `empty(width, height, channel, dtype, storage=None)` | Create empty volume |
| `import_native(dtype, info)` | Import from native texture info |

#### Methods

| Method | Description |
|--------|-------------|
| `copy_from(arr, sync=False, stream=None)` | Copy data from array/texture |
| `copy_from_array(arr, sync=False, stream=None)` | Copy from numpy array |
| `copy_from_tex(tex, sync=False, stream=None)` | Copy from another texture |
| `copy_to(arr, sync=True, stream=None)` | Copy to numpy array |
| `copy_to_tex(tex, sync=False, stream=None)` | Copy to another texture |

### `Texture3DType`

Type descriptor for 3D textures.

```python
class Texture3DType:
    def __init__(self, dtype, channel)
```

#### Attributes

| Attribute | Description |
|-----------|-------------|
| `dtype` | Element data type |
| `channel` | Number of channels |
| `vectype` | Vector type for voxels |
| `luisa_type` | Luisa Compute type |

---

## Shaders

The `shader` module provides shader loading and dispatch functionality.

### `Shader`

A wrapper for compiled compute shaders.

```python
class Shader:
    def __init__(self, shader_path)
    def __call__(self, *args, dispatch_size=None)
```

#### Constructor
```python
Shader(shader_path)
```
- **Parameters:**
  - `shader_path`: Path to the compiled shader file

#### Methods

| Method | Description |
|--------|-------------|
| `__call__(*args, dispatch_size=None)` | Dispatch the shader |

#### Dispatch Parameters

When calling a shader:
- `*args`: Arguments to pass to the shader (buffers, textures, scalars)
- `dispatch_size`: Execution grid size as:
  - `int`: 1D dispatch size
  - `(x,)` or `(x, y)` or `(x, y, z)`: Multi-dimensional dispatch size

#### Example

```python
from robocute.rbc_ext.luisa import init, Shader

init()
shader = Shader("path/to/shader.shader")
shader(buffer, image, dispatch_size=(256, 256, 1))
```

---

## Utilities

### Frame Rate Measurement

The `framerate` module provides frame rate tracking.

#### `FrameRate`

```python
class FrameRate:
    def __init__(self, history_size: int)
    def clear(self) -> None
    def duration(self) -> float
    def record(self, frame_count: int = 1) -> None
    def report(self) -> float
```

#### Methods

| Method | Description |
|--------|-------------|
| `clear()` | Reset the history |
| `duration()` | Get time since last record |
| `record(frame_count=1)` | Record a frame |
| `report()` | Get average FPS over history |

### Vector Swizzling

The `vector` module provides swizzling utilities.

#### `is_swizzle_name(sw)`
Check if string is a valid swizzle (e.g., "xyz", "wzyx").
```python
def is_swizzle_name(sw: str) -> bool
```

#### `get_swizzle_code(sw, maxlen)`
Convert swizzle string to code.
```python
def get_swizzle_code(sw: str, maxlen: int) -> int
```

#### `get_swizzle_resulttype(dtype, length)`
Get result type of swizzle operation.
```python
def get_swizzle_resulttype(dtype, length)
```

## Global Variables

The `globalvars` module contains internal global state:

| Variable | Description |
|----------|-------------|
| `current_context` | Current compilation context |
| `device` | Global device instance |
| `saved_shader_count` | Counter for saved shaders |

#### `get_global_device()`
Get the global device instance.
```python
def get_global_device()
```

---

## Exported Names

The following names are available from the main `luisa` module (`robocute.rbc_ext.luisa`):

### Core Functions
- `init`, `del_device`, `synchronize`, `execute`
- `set_log_callback`
- `verbose`, `info`, `warning`, `error`
- `verbose_with_location`, `info_with_location`, `warning_with_location`, `error_with_location`

### Log Level Constants
- `log_level_verbose`, `log_level_info`, `log_level_warning`, `log_level_error`

### Data Types
- Scalar markers: `half`, `short`, `ushort`
- Vectors: `uint2`, `uint3`, `uint4`, `float2`, `float3`, `float4`, `double2`, `double3`, `double4`, `half2`, `short2`, `ushort2`, `half3`, `short3`, `ushort3`, `half4`, `short4`, `ushort4`

### Buffers
- `buffer`, `Buffer`, `ByteBuffer`
- `BufferType`, `ByteBufferType`

### Images/Textures
- `image2d`, `Image2D`, `Texture2DType`
- `image3d`, `Image3D`, `Texture3DType`

### Shaders
- `Shader`

### Arrays & Structures
- `array`, `Array`, `ArrayType`, `SharedArrayType`
- `struct`, `Struct`, `StructType`, `CustomType`

### Math Types (from `mathtypes`)
- `int2`, `bool2`, `float2`, `int3`, `bool3`, `float3`, `int4`, `bool4`, `float4`
- `float2x2`, `float3x3`, `float4x4`
- `make_int2`, `make_bool2`, `make_float2`, `make_int3`, `make_bool3`, `make_float3`, `make_int4`, `make_bool4`, `make_float4`
- `make_float2x2`, `make_float3x3`, `make_float4x4`

### Utilities
- `capsule_vector`
