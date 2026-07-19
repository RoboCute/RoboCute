---
name: mesh_builder
description: Python mesh construction using MeshBuilder class for RoboCute. Use when creating or modifying mesh data with positions, normals, tangents, UVs, and triangle indices. Provides numpy-based buffer manipulation for efficient mesh generation.
---

# Mesh Builder

Python implementation of rbc::MeshBuilder for constructing mesh data efficiently using numpy arrays.

## Quick Start

```python
import numpy as np
from samples.mesh_builder import MeshBuilder

# Create a simple triangle mesh
builder = MeshBuilder(
    vertex_count=3,
    triangle_count=1,
    uv_count=1,
    has_normal=True,
    has_tangent=False
)

# Set vertex positions
builder.set_position(0, (0.0, 0.0, 0.0))
builder.set_position(1, (1.0, 0.0, 0.0))
builder.set_position(2, (0.5, 1.0, 0.0))

# Set normals
builder.set_normal(0, (0.0, 0.0, 1.0))
builder.set_normal(1, (0.0, 0.0, 1.0))
builder.set_normal(2, (0.0, 0.0, 1.0))

# Set UVs
builder.set_uv(0, 0, (0.0, 0.0))
builder.set_uv(1, 0, (1.0, 0.0))
builder.set_uv(2, 0, (0.5, 1.0))

# Set triangle indices
builder.set_triangle(0, 0, 1, 2)

# Get the mesh resource
mesh = builder.get_mesh()
```

## Initialization

```python
MeshBuilder(
    vertex_count: int,          # Number of vertices (required, > 0)
    triangle_count: int,        # Number of triangles (required, >= 0)
    submesh_offsets: np.ndarray | None = None,  # Triangle offsets for submeshes
    uv_count: int = 0,          # Number of UV sets (>= 0)
    has_normal: bool = False,   # Whether to allocate normal buffer
    has_tangent: bool = False   # Whether to allocate tangent buffer
)
```

## Buffer Access Patterns

### Individual Element Access

```python
# Positions (float4 per vertex, w is padding)
builder.set_position(vertex_index, (x, y, z))

# Normals (float4 per vertex, only if has_normal=True)
builder.set_normal(vertex_index, (nx, ny, nz))

# Tangents (float4 per vertex, only if has_tangent=True)
builder.set_tangent(vertex_index, (tx, ty, tz, tw))

# UVs (float2 per vertex per UV set)
builder.set_uv(vertex_index, uv_set_index, (u, v))

# Triangle indices (3 uint32 per triangle)
builder.set_triangle(triangle_index, i0, i1, i2)
```

### Bulk Array Access (More Efficient)

```python
# Set all positions at once - shape (vertex_count, 3)
positions = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float32)
builder.set_positions(positions)

# Set all normals at once - shape (vertex_count, 3)
normals = np.array([[0, 0, 1], [0, 0, 1], [0, 0, 1]], dtype=np.float32)
builder.set_normals(normals)

# Set all tangents at once - shape (vertex_count, 4)
tangents = np.array([[1, 0, 0, 1], [1, 0, 0, 1], [1, 0, 0, 1]], dtype=np.float32)
builder.set_tangents(tangents)

# Set all UVs for a UV set - shape (vertex_count, 2)
uvs = np.array([[0, 0], [1, 0], [0.5, 1]], dtype=np.float32)
builder.set_uvs(uv_set_index, uvs)

# Set all triangles at once - shape (triangle_count, 3)
triangles = np.array([[0, 1, 2]], dtype=np.uint32)
builder.set_triangles(triangles)
```

## Direct Numpy Array Access

For maximum performance, access buffer arrays directly:

```python
# Position buffer is float4 per vertex (x, y, z, w=0)
# Access as flattened array: vertex i starts at index i*4
start = i * 4
builder.position[start : start+3] = [x, y, z]

# Normal buffer (if enabled)
builder.normal[start : start+3] = [nx, ny, nz]

# Tangent buffer (if enabled)
builder.tangent[start : start+4] = [tx, ty, tz, tw]

# UV buffers list - each is float2 per vertex
uv_start = i * 2
builder.uvs[uv_set_index][uv_start : uv_start+2] = [u, v]

# Triangle indices - 3 indices per triangle
tri_start = tri * 3
builder.triangle_indices[tri_start : tri_start+3] = [i0, i1, i2]
```

## Submeshes

```python
# Create mesh with 2 submeshes
submesh_offsets = np.array([0, 10], dtype=np.uint32)  # First submesh: triangles 0-9, second: 10+
builder = MeshBuilder(
    vertex_count=100,
    triangle_count=20,
    submesh_offsets=submesh_offsets,
    uv_count=1,
    has_normal=True
)

# Get submesh count
print(builder.submesh_count())  # 2
```

## Validation

```python
# Check mesh validity
error_msg = builder.check()
if error_msg:
    print(f"Mesh validation failed: {error_msg}")
else:
    print("Mesh is valid")
```

## Tangent Calculation

```python
# Calculate tangents using Mikktspace algorithm
from samples.mesh_builder import MeshBuilder

tangents = MeshBuilder.calculate_tangent(
    positions=positions_array,    # shape (N, 3)
    uvs=uvs_array,               # shape (N, 2)
    triangles=triangles_array,   # shape (M, 3)
    tangent_w=1.0                # handedness
)
# Returns array of shape (N, 4) with (tx, ty, tz, tw)
```

## Complete Example: Quad Mesh

```python
import numpy as np
from samples.mesh_builder import MeshBuilder

# Create a quad (2 triangles, 4 vertices)
builder = MeshBuilder(
    vertex_count=4,
    triangle_count=2,
    uv_count=1,
    has_normal=True,
    has_tangent=True
)

# Set positions (counter-clockwise)
# bottom-left, bottom-right, top-right, top-left
positions_data = "[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]"
builder.set_positions(np.array(eval(f"[{positions_data}]"), dtype=np.float32))

# Set normals (all facing +Z)
normals_data = "[0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1]"
builder.set_normals(np.array(eval(f"[{normals_data}]"), dtype=np.float32))

# Calculate and set tangents
# Use slice notation to extract xyz from float4 position buffer
positions_xyz = builder.position.reshape(-1, 4)  # shape (4, 4)
positions_xyz = positions_xyz[:, 0:3]  # take columns 0-2, shape (4, 3)

uvs_data = "[0, 0], [1, 0], [1, 1], [0, 1]"
triangles_data = "[0, 1, 2], [0, 2, 3]"
tangents = MeshBuilder.calculate_tangent(
    positions_xyz,
    np.array(eval(f"[{uvs_data}]"), dtype=np.float32),
    np.array(eval(f"[{triangles_data}]"), dtype=np.uint32)
)
builder.set_tangents(tangents)

# Set UVs
builder.set_uvs(0, np.array(eval(f"[{uvs_data}]"), dtype=np.float32))

# Set triangles (counter-clockwise winding)
builder.set_triangles(np.array(eval(f"[{triangles_data}]"), dtype=np.uint32))

# Validate
if not builder.check():
    mesh = builder.get_mesh()
    print(f"Created mesh: {builder}")
```

## Important Notes

- **Position format**: Stored as float4 (x, y, z, 0.0) for GPU alignment
- **Normal format**: Stored as float4 (nx, ny, nz, 0.0) for GPU alignment
- **Tangent format**: Stored as float4 (tx, ty, tz, tw) where tw is handedness
- **UV format**: Stored as float2 (u, v)
- **Index format**: uint32, 3 per triangle
- **Winding order**: Use counter-clockwise for front-facing triangles
- Buffer arrays are numpy views into mesh host buffers - modifications are immediate
