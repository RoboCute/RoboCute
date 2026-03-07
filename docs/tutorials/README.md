# RoboCute Tutorials Summary / 教程概览

This document provides a summary of all available tutorials for RoboCute. 本文档提供 RoboCute 所有可用教程的概览。

---

## Tutorial List / 教程列表

### 1. Graphics Scene Basics / 图形场景基础
**File:** `graphics-scene-basics.md`

**Topics Covered:**
- Creating a 3D graphics scene using RoboCute's low-level graphics API
- Programmatic mesh generation (procedural cubes)
- PBR material setup with textures using OpenPBR standard
- Real-time rendering with path tracing preview
- Geometry buffer export (depth, normal, object ID, etc.)
- Interactive camera control

**Key APIs:**
- `rbc.app.App` - Application singleton
- `re.world.MaterialResource` - PBR materials
- `re.world.MeshResource` - Procedural mesh creation
- `app.ctx.tick()` - Rendering loop

**Sample File:** `samples/app_graphics_scene.py`

---

### 2. Object Movement and Mesh Animation / 物体移动与网格动画
**File:** `object-movement-animation.md`

**Topics Covered:**
- Creating dynamic mesh entities with multiple submeshes
- Setting up PBR materials programmatically
- Animating object position using frame callbacks
- Real-time mesh vertex deformation
- Entity-Component-System (ECS) architecture

**Key APIs:**
- `scene.add_entity()` - Create entities
- `entity.add_component()` - Add components
- `ctx.regist_callback()` / `bind_event()` - Animation callbacks
- `ctx.upload_mesh_data()` - GPU mesh updates

**Sample File:** `samples/app_object_move.py`

---

### 3. Compute Shader Tutorial / 计算着色器教程
**File:** `shader-compute-tutorial.md`

**Topics Covered:**
- Using compute shaders with LuisaCompute integration
- GPU-accelerated image processing
- Loading and executing pre-compiled compute shaders
- Creating and managing GPU images
- GPU synchronization

**Key APIs:**
- `lc.Shader()` - Load compute shaders
- `lc.Image2D.empty()` - Create GPU images
- `lc.execute()` / `lc.synchronize()` - GPU command management

**Sample File:** `samples/app_test_shader.py`

---

### 4. UIPC Physics Integration / UIPC 物理引擎集成
**File:** `uipc-physics-integration.md`

**Topics Covered:**
- Integrating UIPC physics simulation with RoboCute rendering
- Setting up affine body dynamics (ABD) simulation
- Applying external forces to physics bodies
- Creating dynamic mesh visualization from physics state
- Animation callbacks for physics control

**Key Concepts:**
- Affine Body Constitution for rigid body physics
- 12D force vectors (3D translation + 9D shape velocity)
- Simplicial surface extraction
- Physics-rendering synchronization

**Sample File:** `samples/app_uipc_physics.py`

---

### 5. UIPC ABD+FEM Physics Simulation / UIPC ABD+FEM 物理模拟
**File:** `uipc-abd-fem-simulation.md`

**Topics Covered:**
- Mixed ABD (Affine Body Dynamics) and FEM (Finite Element Method) simulation
- Creating soft deformable bodies vs rigid-like bodies
- Synchronizing physics simulation state with rendered geometry
- Screenshot capture during simulation
- Decoupled physics and rendering rates

**Key Concepts:**
- StableNeoHookean constitution for FEM
- Elastic moduli (Young's modulus, Poisson's ratio)
- Contact models and friction
- Dynamic mesh updates from simulation surface

**Sample File:** `samples/app_uipc_abd_fem.py`

---

### 6. URDF Robot Arm Visualization / URDF 机械臂可视化
**File:** `urdf-robot-arm.md`

**Topics Covered:**
- Creating URDF (Unified Robot Description Format) files programmatically
- Parsing URDF XML to extract link and joint information
- Generating meshes for cylinders and boxes
- Building hierarchical robot arm scenes
- Joint animation with sine wave patterns
- Axis-angle to quaternion conversion

**Key Concepts:**
- URDF structure (links, joints, geometries)
- Hierarchical transform chains
- Revolute joint animation
- Procedural mesh generation for basic shapes

**Sample File:** `samples/app_urdf.py`

---

## Learning Path / 学习路径

### Beginner / 初级
1. **Graphics Scene Basics** - Learn the fundamentals of RoboCute rendering
2. **Compute Shader Tutorial** - Understand GPU compute basics

### Intermediate / 中级
3. **Object Movement and Mesh Animation** - Learn animation and dynamic meshes
4. **URDF Robot Arm Visualization** - Build complex hierarchical scenes

### Advanced / 高级
5. **UIPC Physics Integration** - Add physics simulation to your scenes
6. **UIPC ABD+FEM Simulation** - Advanced physics with deformable bodies

---

## Common Prerequisites / 通用前置条件

Most tutorials require:
- RoboCute installed and built from source
- A valid RoboCute project directory with `rbc_project.json`
- Python 3.x with numpy

Physics tutorials additionally require:
- UIPC Python bindings (`pip install uipc`)

---

## Quick Reference / 快速参考

| Tutorial | Backend | Key Feature |
|----------|---------|-------------|
| Graphics Scene Basics | DX/Vulkan | Procedural meshes, PBR materials |
| Object Movement | DX/Vulkan | Animation callbacks, vertex deformation |
| Compute Shader | DX/Vulkan | GPU compute, image processing |
| UIPC Physics | DX/Vulkan | Rigid body physics, external forces |
| UIPC ABD+FEM | DX/Vulkan | Soft body physics, deformable objects |
| URDF Robot Arm | DX/Vulkan | Robot visualization, joint animation |

---

*Last updated: 2026-03-07*
