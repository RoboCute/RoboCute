# Graphics Scene Basics / 图形场景基础

This tutorial walks you through creating a 3D graphics scene using RoboCute's low-level graphics API. You'll learn how to create meshes programmatically, apply materials, set up rendering, and export geometry data.

本教程将指导您使用 RoboCute 的低级图形 API 创建 3D 图形场景。您将学习如何以编程方式创建网格、应用材质、设置渲染以及导出几何数据。

## Overview / 概述

The `app_graphics_scene.py` sample demonstrates:

- Creating a graphics application with custom backend (DX/Vulkan)
- Programmatic mesh generation (procedural cubes)
- PBR material setup with textures
- Real-time rendering with path tracing preview
- Geometry buffer export (depth, normal, object ID, etc.)
- Interactive camera control
- Command-line interface integration

---

## Prerequisites / 前置条件

Before starting this tutorial, ensure you have:

1. RoboCute installed and built from source (see [BUILD.md](../BUILD.md))
2. A valid RoboCute project directory with `rbc_project.json`
3. A texture file (e.g., `test_grid.png`) in your project

---

## Step 1: Application Setup / 应用程序设置

First, let's set up the RoboCute application with the desired graphics backend.

```python
import os
import time
from pathlib import Path
import numpy as np
import argparse
from PIL import Image

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
from robocute.rbc_ext._C import lcapi_c as lcapi

# Global app reference
app: rbc.app.App = None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b", "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-p", "--project",
        type=str,
        help="rbc project path, the directory containing rbc_project.json",
        required=True,
    )
    args = parser.parse_args()

    project_path = Path(args.project)
    
    # Initialize the application
    global app
    app = rbc.app.App()  # rbc app singleton
    app.init(project_path=project_path, backend_name=args.backend)
    
    # Check if context is valid
    if not app.ctx:
        print("Context not Valid!")
        return
```

### Key Concepts / 关键概念

- **`rbc.app.App`**: The main application singleton that manages the graphics context, scene, and rendering
- **Backend**: Choose between `"dx"` (DirectX 12) or `"vk"` (Vulkan) depending on your platform
- **Project Path**: Points to a directory containing `rbc_project.json` which defines project settings and assets

---

## Step 2: Display Initialization / 显示初始化

Set up the display window with a specific resolution and configure the camera.

```python
    # Set display resolution
    resolution = lc.uint2(1920, 1080)
    app.init_display(resolution.x, resolution.y)
    
    if not app.display_cam:
        print("Display not Valid!")
        return

    # Configure camera transform
    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 0, -1), False)
    
    # Enable interactive camera control (mouse/keyboard)
    app.ctx.enable_camera_control()
```

### Key Concepts / 关键概念

- **`init_display()`**: Creates the render window with specified dimensions
- **`display_cam`**: The camera used for rendering the scene
- **`get_display_transform()`**: Gets the camera's transform component for positioning
- **`enable_camera_control()`**: Allows users to navigate the scene with mouse and keyboard

---

## Step 3: Creating Materials / 创建材质

Materials in RoboCute use the OpenPBR standard. Let's create two PBR materials:

```python
import mat_builtin as mat

def make_cube_mesh(scene: re.world.Scene, tex: re.world.TextureResource):
    # Create first material - white rough material
    mat0 = re.world.MaterialResource()
    mat0_json = mat.OpenPBRInterface(app._project)
    mat0_json.set_specular_roughness(0.8)      # Rough surface
    mat0_json.set_weight_metallic(0.3)          # Slightly metallic
    mat0_json.set_base_albedo((0.8, 0.8, 0.8))  # White color
    mat0_json.set_base_albedo_tex(tex)          # Apply texture
    mat0.load_from_json(mat0_json.dump_to_json())

    # Create second material - green metallic material
    mat1 = re.world.MaterialResource()
    mat1_json = mat.OpenPBRInterface(app._project)
    mat1_json.set_specular_roughness(0.5)
    mat1_json.set_weight_metallic(0.3)
    mat1_json.set_base_albedo((0.140, 0.450, 0.091))  # Green color
    mat1_json.set_base_albedo_tex(tex)
    mat1.load_from_json(mat1_json.dump_to_json())
    # ...
```


## Step 4: Procedural Mesh Generation / 程序化网格生成

Let's create a mesh with two cubes programmatically. This involves defining vertices, UVs, and triangle indices.

### Vertex and Triangle Counts

```python
vertex_count = 16    # 8 vertices per cube × 2 cubes
triangle_count = 24  # 12 triangles per cube × 2 cubes
```

### Creating the Mesh Resource

```python
# def make_cube_mesh(scene: re.world.Scene, tex: re.world.TextureResource):
# ...
    # Create material handle vector
    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(mat0._handle)
    mat_vector.emplace_back(mat1._handle)
    
    # Create entity
    entity = scene.add_entity()
    entity.set_name("test_cube")
    
    # Add components
    trans = re.world.TransformComponent(
        entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(
        entity.add_component("RenderComponent"))
    
    trans.set_pos(lc.double3(0, -1, 1), False)
    trans.set_rotation(lc.float4(0, -1, 0, 0), False)
    
    # Create mesh resource
    cube_mesh = re.world.MeshResource()
    submesh_offsets = np.empty(shape=2, dtype=np.uint32)
    submesh_offsets[0] = 0                   # First submesh starts at 0
    submesh_offsets[1] = triangle_count // 2  # Second submesh starts at triangle 12
    
    cube_mesh.create_empty(
        submesh_offsets, vertex_count, triangle_count, 1, False, False
    )
```

### Data Layout / 数据布局

The mesh data is stored in a contiguous buffer with the following layout:

```
[position_data]         : vertex_count × 4 floats (x, y, z, _align)
[normal(optional)]      : vertex_count × 4 floats (x, y, z, _align)
[tangent(optional)]     : vertex_count × 4 floats (x, y, z, _align)
[uv0(optional)]         : vertex_count × 2 floats (u, v)
[uv1(optional)]         : vertex_count × 2 floats (u, v)
[index_data]            : triangle_count × 3 uint32 (i0, i1, i2)
```

### Filling Vertex Data / 填充顶点数据

```python
def create_mesh_array(mesh_array):
    expected_size = vertex_count * 4 + vertex_count * 2 + triangle_count * 3
    
    # Position data
    vertex_arr = np.ndarray(
        vertex_count * 4, dtype=np.float32, buffer=mesh_array.data
    )
    
    # UV data
    uv_arr = np.ndarray(
        vertex_count * 2, dtype=np.float32,
        buffer=mesh_array.data,
        offset=vertex_arr.size * vertex_arr.itemsize
    )
    
    # Index data
    indices_arr = np.ndarray(
        shape=triangle_count * 3, dtype=np.uint32,
        buffer=mesh_array.data,
        offset=vertex_arr.size * vertex_arr.itemsize + 
              uv_arr.size * uv_arr.itemsize
    )
    
    # Define cube vertices (8 corners)
    def push_vert():
        # Cube vertices with current offset and scale
        vertices = [
            (-0.5, -0.5, -0.5),  # 0: left-bottom-back
            (-0.5, -0.5,  0.5),  # 1: left-bottom-front
            ( 0.5, -0.5, -0.5),  # 2: right-bottom-back
            ( 0.5, -0.5,  0.5),  # 3: right-bottom-front
            (-0.5,  0.5, -0.5),  # 4: left-top-back
            (-0.5,  0.5,  0.5),  # 5: left-top-front
            ( 0.5,  0.5, -0.5),  # 6: right-top-back
            ( 0.5,  0.5,  0.5),  # 7: right-top-front
        ]
        for x, y, z in vertices:
            vec = lc.float4(x, y, z, 0) * scale + offset
            for i in range(4):
                vertex_arr[vert_size + i] = vec[i]
            vert_size += 4
```

### Defining Triangles / 定义三角形

A cube has 6 faces, each composed of 2 triangles (12 triangles total):

```python
    def push_cube_triangles():
        # Bottom face
        push_indices(0); push_indices(1); push_indices(2)
        push_indices(1); push_indices(3); push_indices(2)
        # Top face
        push_indices(4); push_indices(5); push_indices(6)
        push_indices(5); push_indices(7); push_indices(6)
        # Left face
        push_indices(0); push_indices(1); push_indices(4)
        push_indices(1); push_indices(5); push_indices(4)
        # Right face
        push_indices(2); push_indices(3); push_indices(6)
        push_indices(3); push_indices(7); push_indices(6)
        # Back face
        push_indices(0); push_indices(2); push_indices(4)
        push_indices(2); push_indices(6); push_indices(4)
        # Front face
        push_indices(1); push_indices(3); push_indices(5)
        push_indices(3); push_indices(7); push_indices(5)
```

---

## Step 5: Rendering Loop / 渲染循环

The main rendering loop handles frame updates and rendering:

```python
    frame_index = 0
    tick_stage = re.world.TickStage.PathTracingPreview
    
    while not app.ctx.should_close():
        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time
        
        # Update frame index for temporal effects
        app.display_cam.set_frame_index(frame_index)
        
        # Tick the rendering context
        if app.ctx.tick(delta_time, tick_stage, True) or app._requires_reset:
            frame_index = 0
            app._requires_reset = False
        else:
            frame_index += 1
```

### Tick Stages / 渲染阶段

| Stage | Description |
|-------|-------------|
| `PathTracingPreview` | Real-time path tracing with progressive refinement |
| `Rasterization` | Fast rasterization for editing |
| `OfflineRendering` | High-quality offline rendering |

---

## Step 6: Geometry Export / 几何导出

RoboCute can export various geometry buffers for further processing:

```python
    # Setup geometry export buffer
    geometry_buffer = lc.Buffer(
        resolution.x * resolution.y * (1 + 3 + 4 + 3 + 3), float
    )
    
    app.display_cam.set_geometry_export_buffer(
        geometry_buffer.info(),
        re.world.RendererGeometryType(
            int(re.world.RendererGeometryType.Depth)
            | int(re.world.RendererGeometryType.Normal)
            | int(re.world.RendererGeometryType.ObjectID)
            | int(re.world.RendererGeometryType.PrimID)
            | int(re.world.RendererGeometryType.Barycentric)
            | int(re.world.RendererGeometryType.Emission)
            | int(re.world.RendererGeometryType.Albedo)
        ),
    )
```

### Available Geometry Types / 可用几何类型

| Type | Channels | Description |
|------|----------|-------------|
| `Depth` | 1 float | Distance from camera |
| `Normal` | 3 floats | Surface normal vector |
| `ObjectID` | 1 uint32 | Entity ID per pixel |
| `PrimID` | 1 uint32 | Primitive ID per pixel |
| `Barycentric` | 2 floats | Barycentric coordinates |
| `Emission` | 3 floats | Emissive color |
| `Albedo` | 3 floats | Base color |

### Reading and Saving Export Data / 读取和保存导出数据

```python
    # After rendering enough frames (e.g., 128)
    if frame_index == 128:
        # Copy data from GPU buffer
        expected_size = resolution.x * resolution.y * (1 + 3 + 4 + 3 + 3)
        geometry_array = np.empty(shape=expected_size, dtype=np.float32)
        geometry_buffer.copy_to(geometry_array)
        
        # Parse the data
        offset = 0
        pixel_size = resolution.x * resolution.y
        
        depth_array = geometry_array[offset:offset + pixel_size]
        offset += pixel_size
        
        normal_array = geometry_array[offset:offset + pixel_size * 3]
        offset += pixel_size * 3
        
        object_id_array = geometry_array[offset:offset + pixel_size].view(dtype=np.uint32)
        # ... continue for other channels
        
        # Save as images
        depth_img = depth_array.reshape(height, width)
        depth_pil = Image.fromarray(depth_norm.astype(np.uint8), mode='L')
        depth_pil.save("depth.png")
```

---

## Step 7: Running the Application / 运行应用程序

Putting it all together:

```python
def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("-b", "--backend", type=str, default="dx")
    parser.add_argument("-p", "--project", type=str, required=True)
    parser.add_argument("-o", "--output", action="store_true", help="Export")
    args = parser.parse_args()

    # Initialize app
    global app
    app = rbc.app.App()
    app.init(project_path=Path(args.project), backend_name=args.backend)
    
    # Import texture
    tex = app._project.import_texture('test_grid.png', 4, True)
    
    # Setup display
    resolution = lc.uint2(1920, 1080)
    app.init_display(resolution.x, resolution.y)
    app.ctx.enable_camera_control()
    
    # Create scene object
    entity = make_cube_mesh(app.scene, tex=tex)
    
    # Main loop
    frame_index = 0
    last_time = time.time()
    
    while not app.ctx.should_close():
        # ... render loop ...
        pass

if __name__ == "__main__":
    main()
```

---

## Running the Sample / 运行示例

```bash
# Run with DirectX backend
python samples/app_graphics_scene.py -p /path/to/project -b dx

# Run with Vulkan backend
python samples/app_graphics_scene.py -p /path/to/project -b vk

# Run with geometry export
python samples/app_graphics_scene.py -p /path/to/project -o
```

---

## Summary / 总结

In this tutorial, you learned how to:

1. ✅ Initialize a RoboCute graphics application
2. ✅ Create PBR materials with the OpenPBR interface
3. ✅ Generate procedural meshes with custom geometry
4. ✅ Set up a real-time rendering loop with path tracing
5. ✅ Export geometry buffers for further processing
6. ✅ Handle interactive camera controls

### Next Steps / 下一步

- Explore the [Node System](../user-guide/node-system.md) for visual graph editing
- Learn about [Animation](../user-guide/animation.md) to bring your scenes to life
- Check out [Scene Management](../user-guide/scene-management.md) for more complex scenes

---

## Complete Code / 完整代码

The complete sample code is available at:
- `samples/app_graphics_scene.py`

Key dependencies:
- `samples/cli.py` - CLI integration module
- `mat_builtin.py` - Built-in material utilities
