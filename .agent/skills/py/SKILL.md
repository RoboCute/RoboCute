---
name: py
---
# RoboCute Python API

RoboCute Python API provides access to the rendering engine via generated bindings.

## Module Imports

```python
import robocute as rbc                    # Main app
import robocute.rbc_ext as re             # World resources
import robocute.rbc_ext.luisa as lc       # Math types, buffers, shaders
import robocute.rbc_ext.luisa as lcapi    # Low-level API types
```

## App Initialization

```python
app = rbc.app.App()  # Singleton
app.init(project_path=Path(...), backend_name="dx")  # or "vk"
app.init_display(width, height)
app.ctx.enable_camera_control()

# Camera transform
transform = app.get_display_transform()
transform.set_pos(lc.double3(0, 0, -1), recursive=False)
```

## Class Hierarchy

All classes wrap a C++ handle (`_handle`) and provide:
- `__bool__()`: Check validity
- `dispose()`: Explicit cleanup (most also have `__del__`)

### Core Classes

| Class | Purpose | Key Methods |
|-------|---------|-------------|
| `Object` | Base | `guid()`, `type_name()`, `is_type(name)` |
| `Entity` | Scene object | `add_component(name)`, `get_component(name)`, `set_name(name)` |
| `Component` | Entity component | `entity()`, `dispose()` |
| `TransformComponent` | Position/rotation/scale | `set_pos()`, `set_rotation()`, `set_scale()`, `position()`, `rotation()` |
| `RenderComponent` | Mesh + materials | `update_mesh(mesh)`, `update_material(mat_vector)`, `mesh()` |
| `DataComponent` | Custom data + events | `set_info(name, data)`, `bind_event(event_type, callback_name)` |
| `LightComponent` | Lighting | `add_point_light()`, `add_spot_light()`, `add_area_light()` |
| `CameraComponent` | Camera | `set_fov()`, `set_focus_distance()`, `render_settings()` |
| `SkelMeshComponent` | Skeletal mesh | `SetRefSkelMesh(skel_mesh)`, `tick(dt)` |

### Resources (inherit from `Resource`)

| Class | Purpose | Key Methods |
|-------|---------|-------------|
| `Scene` | Entity container | `add_entity()`, `get_entity_by_name()`, `remove_entity()` |
| `MeshResource` | Geometry | `create_empty()`, `data_buffer()`, `pos_buffer()`, `install()` |
| `MaterialResource` | PBR material | `load_from_json(json_str)`, `dump_json()` |
| `TextureResource` | Texture | `create_empty()`, `set_skybox()`, `size()` |
| `BufferResource` | GPU buffer | `create_empty()`, `host_data()` |
| `SkelMeshResource` | Skinned mesh | `ref_skeleton()`, `ref_skin()` |
| `SkeletonResource` | Bone hierarchy | `ref_skel()` |
| `SkinResource` | Skinning data | `ref_mesh()`, `ref_skel()` |
| `AnimSequenceResource` | Animation clip | `ref_seq()`, `ref_skel()` |
| `AnimGraphResource` | Animation state | `create_simple_anim_graph(anim_seq)` |
| `VoxelResource` / `SDFVoxelResource` | Voxel data | `create_empty()` |

### Other Types

| Class | Purpose |
|-------|---------|
| `Project` | Asset management: `init(assets_root)`, `import_mesh()`, `import_texture()`, `import_scene()` |
| `RBCContext` | Render context: `tick()`, `upload_mesh_data()`, `should_close()` |
| `RenderSettings` | Post-process: `set_offline_spp()`, `set_denoise()`, `set_global_exposure()` |
| `BasicData` | Variant type: `set_int()`, `set_float()`, `set_string()`, `get_resource()` |
| `SelectQuery` | Raycast results: `valid()`, `get_component()`, `prim_id()`, `barycentric()` |
| `EntitiesCollection` | Entity list: `count()`, `get_entity(i)` |

## Math Types (from `lcapi`)

```python
lc.double3(x, y, z)   # Position
lc.float4(x, y, z, w) # Quaternion rotation
lc.float3(r, g, b)    # Color
lc.uint2(w, h)        # Resolution
lc.float4x4(...)      # Matrix
lc.capsule_vector()   # Material vector for render component
```

## Creating a Mesh Entity

```python
# 1. Create materials
mat0 = re.world.MaterialResource()
mat0.load_from_json(mat_json)

mat_vector = lc.capsule_vector()
mat_vector.emplace_back(mat0._handle)

# 2. Create entity
entity = scene.add_entity()
entity.set_name("my_mesh")
trans = re.world.TransformComponent(entity.add_component("TransformComponent"))
render = re.world.RenderComponent(entity.add_component("RenderComponent"))
trans.set_pos(lc.double3(0, 0, 0), False)

# 3. Create mesh with numpy
mesh = re.world.MeshResource()
submesh_offsets = np.array([0, triangle_count // 2], dtype=np.uint32)
mesh.create_empty(submesh_offsets, vertex_count, triangle_count, uv_count=1, 
                  contained_normal=False, contained_tangent=False)

# 4. Fill data
arr = np.ndarray(vertex_count * 4 + triangle_count * 3, 
                 dtype=np.float32, buffer=mesh.data_buffer())
# ... fill vertex/UV/index data ...

mesh.install()
render.update_object(mat_vector, mesh)
```

## Event Callbacks

```python
def my_callback(ptr):
    comp = re.world.DataComponent(ptr)
    entity = comp.entity()
    # ... modify transform/mesh ...
    ctx.upload_mesh_data(mesh)  # Sync GPU after CPU modification

# Register and bind
ctx.regist_callback("my_callback", my_callback)
data_comp = re.world.DataComponent(entity.add_component("DataComponent"))
data_comp.bind_event(re.world.DataComponentEventType.BeforeFrame, "my_callback")
```

## Main Loop

```python
# Option 1: Built-in loop
app.set_user_callback(tick_logic)  # Called every frame
app.run()

# Option 2: Manual loop
while not ctx.should_close():
    ctx.tick(delta_time, re.world.TickStage.PathTracingPreview, prepare_denoise=True)
```

## Importing Assets

```python
# Via project
project = app._project
tex = project.import_texture('path.png', mip_level=1, to_vt=False)
mesh = project.import_mesh('path.fbx')
scene = project.import_scene('path.gltf', extra_meta='{}')
```

## Important Notes

- **Generated code**: Files in `robocute.rbc_ext.generated` are auto-generated from `src/rbc_meta/`
- **Handle safety**: Always check `if obj:` before using `_handle`
- **Mesh updates**: Call `ctx.upload_mesh_data(mesh)` after CPU-side modifications
- **Install resources**: Call `resource.install()` before using in render
- **Cleanup**: Call `dispose()` or `del obj` to release C++ resources
