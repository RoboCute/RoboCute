# API & Programming Structure Comparison: Polyscope vs RoboCute

## Overview

This document compares the API and programming structure between **polyscope** and **rbc (RoboCute)** for 3D visualization, based on UIPC physics simulation examples.

---

## 1. Initialization & Setup

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Entry Point** | `polyscope.init()` | `app = rbc.app.App()` + `app.init(...)` |
| **Configuration** | Minimal, global settings | Project-based with backend selection (DX/VK), requires `rbc_project.json` |
| **Display Setup** | Automatic | Manual: `app.init_display(width, height, title)` |
| **Window Control** | `polyscope.show()` blocks until close | Manual loop with `app.ctx.should_close()` |

### Polyscope - Simple
```python
polyscope.init()
polyscope.set_ground_plane_mode('shadow_only')
# ... setup ...
polyscope.set_user_callback(tick_function)
polyscope.show()
```

### RoboCute - Explicit
```python
app = rbc.app.App()
polyscope.set_ground_plane_mode('shadow_only', scale=100, height=0) # scale: plane size,  height: y-axis value
app.set_user_callback(tick_function)
app.init(project_path=path, backend_name="dx")
app.init_display(1920, 1080, "Title")
app.
while not app.ctx.should_close():
    app.ctx.tick(delta_time, tick_stage, True)
```

---

## 2. Architecture Paradigm

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Pattern** | Functional/Procedural | Entity-Component-System (ECS) |
| **Scene Objects** | Register meshes directly | Create entities → add components → attach resources |
| **State Management** | Implicit global state | Explicit `App` singleton with `ctx` (context) |

### Polyscope - Direct registration
```python
mesh, _, _ = sgui.register()
mesh.set_edge_width(1.0)
```

### RoboCute - ECS approach
```python
import samples.mat_builtin as mat

mat0_json = mat.OpenPBRInterface(app._project)
mat0_json.set_specular_roughness(0.5)
mat0_json.set_weight_metallic(0.3)
mat0_json.set_base_albedo((0.8, 0.8, 0.8))  # Blue-ish color
mat0 = re.world.MaterialResource()
mat0.load_from_json(mat0_json.dump_to_json())

mat_vector = lc.capsule_vector()
mat_vector.emplace_back(mat0._handle)

entity = scene.add_entity()
entity.set_name("object")
trans = re.world.TransformComponent(entity.add_component("TransformComponent"))
render = re.world.RenderComponent(entity.add_component("RenderComponent"))
render.update_object(mat_vector, mesh)
```

---

## 3. Mesh Handling

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Mesh Source** | Uses `SceneGUI` from uipc | Manual buffer management |
| **Data Flow** | `sgui.register()` + `sgui.update()` | Direct numpy buffer manipulation + `upload_mesh_data()` |
| **Buffer Access** | Abstracted | Raw: `mesh.data_buffer()`, `pos_buffer()`, `triangle_indices_buffer()` |
| **Topology Updates** | Automatic via `sgui.update()` | Manual check + recreation if needed |

### Polyscope - High level
```python
sio = SceneIO(scene)
sgui = SceneGUI(scene)
mesh, _, _ = sgui.register()
# In loop: just update
sgui.update()
```

### RoboCute - Low level buffer access
```python
mesh = re.world.MeshResource()
mesh.create_empty(submesh_offsets, vcount, tcount, 0, False, False)
mesh_array = np.ndarray(..., buffer=mesh.data_buffer())
# ... fill array ...
mesh.install()
# In loop: manual update
app.ctx.upload_mesh_data(mesh)
```

---

## 4. Rendering & Materials

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Materials** | Basic/Automatic | Full PBR with `OpenPBRInterface` |
| **Properties** | Edge width, color, etc. | Roughness, metallic, albedo, textures |
| **Shading Model** | Simplified | Physically Based Rendering |

### Polyscope - Simple visualization
```python
mesh.set_edge_width(1.0)
```

### RoboCute - Full PBR material system
```python
mat = re.world.MaterialResource()
mat_json = mat.OpenPBRInterface(app._project)
mat_json.set_specular_roughness(0.5)
mat_json.set_weight_metallic(0.3)
mat_json.set_base_albedo((0.5, 0.5, 0.5))
mat.load_from_json(mat_json.dump_to_json())
```

---

## 5. Update Loop & Callbacks

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Callback Style** | `polyscope.set_user_callback(fn)` | `app.set_user_callback(fn)` OR manual loop |
| **Loop Control** | Managed by `polyscope.show()` | Fully manual with `tick()` |
| **Physics Integration** | Simple flag check in callback | Explicit frame pacing with `TickStage` |

### Polyscope - Callback based
```python
run = False
def on_update():
    global run
    if psim.Button('run & stop'):
        run = not run     
    if run:
        world.advance()
        world.retrieve()
        sgui.update()
polyscope.set_user_callback(on_update)
polyscope.show()
```

### RoboCute - Manual loop with explicit timing
```python
tick_stage = re.world.TickStage.PathTracingPreview
while not app.ctx.should_close():
    app.ctx.tick(delta_time, tick_stage, True)
    physics_app.step()  # Physics update
    app.ctx.denoise()   # Post-processing
```

---

## 6. Camera & View Control

| Aspect | Polyscope | RoboCute (rbc) |
|--------|-----------|----------------|
| **Camera** | Automatic orbit controls | Explicit `display_cam` with `TransformComponent` |
| **Positioning** | Default view | Manual: `transform.set_pos()`, `set_rotation()` |
| **Control Enable** | Always on | Opt-in: `app.ctx.enable_camera_control()` |

### Polyscope - Automatic
```python
# (No code needed - orbit controls are default)
```

### RoboCute - Explicit control
```python
transform = app.get_display_transform()
transform.set_pos(lc.double3(6, 12, -15), False)
rot = euler_to_quaternion(degrees_to_radians(15), degrees_to_radians(-15), 0)
transform.set_rotation(lc.float4(rot), False)
app.ctx.enable_camera_control()
```

---

## Summary: When to Use Which

| Use Case | Recommended Choice |
|----------|------------------|
| **Quick debugging/visualization** | Polyscope - Minimal boilerplate, immediate results |
| **Production rendering** | RoboCute - Full control over rendering pipeline, PBR materials |
| **Physics simulation preview** | Polyscope - Easier integration with UIPC's SceneGUI |
| **High-quality export/rendering** | RoboCute - Path tracing, denoising, multi-channel export |
| **Learning/Prototyping** | Polyscope - Simpler mental model |
| **Game-like applications** | RoboCute - ECS architecture scales better |

---

## Key Takeaway

**Polyscope** prioritizes **ease of use** with its "register and show" approach, while **RoboCute** emphasizes **flexibility and quality** through its ECS-based architecture and explicit rendering control.

- Use **Polyscope** when you need quick visualization with minimal code
- Use **RoboCute** when you need production-quality rendering with full control
