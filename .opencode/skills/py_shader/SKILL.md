# Python Shader Skill

Loading and dispatching compute shaders with buffer and image resources.

## Quick Start

```python
import robocute as rbc
import robocute.rbc_ext.luisa as lc

app = rbc.app.App()
app.init("dx", None, ".")  # backend: dx/vk
```

## Shader

### Load

```python
shader = lc.Shader('path/to/shader.bin')  # lazy-loaded on first call
```

### Dispatch

```python
# 1D dispatch
shader(arg1, arg2, dispatch_size=1024)

# 2D/3D dispatch
shader(img, buf, scale, dispatch_size=(width, height))  # z=1 default
shader(img, buf, dispatch_size=(x, y, z))
```

## Buffer Resource

### Create

```python
# Empty buffer
buf = lc.Buffer(size, dtype)  # dtype: int, float, bool, uint, etc.

# From numpy array
buf = lc.Buffer.from_array(arr)  # arr: np.ndarray

# From Python list
buf = lc.Buffer.from_list([1.0, 2.0, 3.0])
```

### Data Transfer

```python
# Upload (Host -> Device)
buf.copy_from(numpy_array)
buf.copy_from_list(python_list)

# Download (Device -> Host)
buf.copy_to(numpy_array)       # sync=True by default
result = buf.numpy()           # returns np.ndarray
```

## Image2D Resource

### Create

```python
# Empty image
img = lc.Image2D.empty(width, height, channel, dtype)
# channel: 1, 2, or 4
# dtype: int, uint, float

# From numpy array (H x W x C)
img = lc.Image2D.from_array(arr)
```

### Data Transfer

```python
# Upload
img.copy_from(numpy_array)

# Download
img.copy_to(numpy_array)
```

## Complete Example

```python
import numpy as np
import robocute as rbc
import robocute.rbc_ext.luisa as lc

# Init
app = rbc.app.App()
app.init("dx", None, ".")

# Load shader
shader = lc.Shader('compute.bin')

# Create resources
width, height = 1024, 1024
img = lc.Image2D.empty(width, height, 4, float)
buf = lc.Buffer(100, float)

# Upload data
input_arr = np.random.rand(height, width, 4).astype(np.float32)
img.copy_from(input_arr)

# Dispatch shader
shader(img, buf, 1.0, dispatch_size=(width, height))

# Download result
output_arr = np.empty(100, dtype=np.float32)
buf.copy_to(output_arr)
```

## Math Types

```python
# Vector types for shader args
lc.uint2(x, y)
lc.uint3(x, y, z)
lc.float2(x, y)
lc.float3(x, y, z)
lc.float4(x, y, z, w)
```
