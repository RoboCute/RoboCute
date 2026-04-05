# RoboCute 骨骼动画 Python 完整指南

## 概述

本文档提供在 RoboCute 中使用 Python 进行骨骼动画开发的完整指南，包括：
- 现有 Python 接口的完整参考
- 完整的动画示例代码解析
- 缺失接口的识别与建议
- 物理动画集成的最佳实践

## 快速开始

### 运行完整示例

```bash
cd <project_root>
uv run python -m samples.complete_anim_example \
    -p d:/ws/repos/RoboCute-repo/rbc-project-anim \
    -g assets/anim_test/test_anim.gltf \
    -b dx
```

### 最简单的动画代码

```python
import robocute.rbc_ext as re
import robocute as rbc

# 1. 初始化
app = rbc.app.App()
app.init_ctx()
app.init_device("dx")
app.init_render()
app.init_world(project_path / "library")

# 2. 创建项目
app._project = re.world.Project()
app._project.init(str(project_path / "assets"))
app._project.scan_project()

# 3. 导入资源
skeleton = app._project.import_skeleton("character.gltf")
skin = app._project.import_skin("character.gltf")
skin.ref_skel = skeleton
skin.generate_LUT()

anim_seq = app._project.import_anim_sequence("character.gltf")
anim_seq.ref_skel = skeleton

# 4. 创建动画图
anim_graph = re.world.AnimGraphResource()
anim_graph.create_simple_anim_graph(anim_seq)

# 5. 创建骨骼网格资源
skel_mesh = re.world.SkelMeshResource()
skel_mesh.ref_skin = skin
skel_mesh.ref_skeleton = skeleton
skel_mesh.ref_anim_graph = anim_graph

# 6. 创建实体
scene = re.world.Scene()
entity = scene.add_entity()

# 添加组件
transform = re.world.TransformComponent(entity.add_component("TransformComponent"))
transform.set_pos(lc.double3(0, 0, 0), True)

render = re.world.RenderComponent(entity.add_component("RenderComponent"))

skelmesh_comp = re.world.SkelMeshComponent(entity.add_component("SkelMeshComponent"))
skelmesh_comp.SetRefSkelMesh(skel_mesh)

# 7. 初始化动画（关键！）
skelmesh_comp.tick(0.0)

# 8. 动画循环
def tick():
    skelmesh_comp.tick(0.016)  # 60fps
    skelmesh_comp.update_render()

app.set_user_callback(tick)
app.run()
```

## Python 接口完整参考

### 资源类 (Resources)

#### SkeletonResource - 骨骼资源

```python
class SkeletonResource(Resource):
    # 基础接口
    def ref_skel() -> VoidPtr: ...
    def log_brief() -> None: ...
    
    # 骨骼层级接口（新增）
    def get_num_joints() -> int: ...
    def get_num_soa_joints() -> int: ...
    def get_joint_names() -> Vector[str]: ...
    def get_joint_parents() -> Vector[int]: ...
    def get_joint_rest_poses() -> Vector[float4x4]: ...
    def get_parent_index(joint_index: int) -> int: ...
```

**使用示例：**

```python
skeleton = project.import_skeleton("model.gltf")

# 获取骨骼数量
num_joints = skeleton.get_num_joints()
print(f"Skeleton has {num_joints} joints")

# 遍历骨骼层级
names = skeleton.get_joint_names()
parents = skeleton.get_joint_parents()
for i, (name, parent) in enumerate(zip(names, parents)):
    parent_name = names[parent] if parent >= 0 else "None"
    print(f"[{i}] {name} (parent: {parent_name})")

# 获取休息姿态矩阵（用于物理初始化）
rest_poses = skeleton.get_joint_rest_poses()
for i, matrix in enumerate(rest_poses[:3]):
    print(f"Joint {i} rest pose:\n{matrix}")
```

#### SkinResource - 蒙皮资源

```python
class SkinResource(Resource):
    def ref_skel() -> SkeletonResource: ...
    def ref_mesh() -> MeshResource: ...
    def generate_LUT() -> None: ...
    def log_brief() -> None: ...
    def JointRemaps() -> Vector[str]: ...
    def InverseBindPoses() -> Vector[float4x4]: ...
    def JointRemapsLUT() -> Vector[uint]: ...
```

**使用示例：**

```python
skin = project.import_skin("model.gltf")
skin.ref_skel = skeleton
skin.ref_mesh = mesh
skin.generate_LUT()  # 必须调用！

# 获取关节映射
joint_names = skin.JointRemaps()
inverse_bind_matrices = skin.InverseBindPoses()

print(f"Skin has {len(joint_names)} joints")
for name, matrix in zip(joint_names[:5], inverse_bind_matrices[:5]):
    print(f"{name}: {matrix}")
```

#### AnimSequenceResource - 动画序列资源

```python
class AnimSequenceResource(Resource):
    def ref_seq() -> AnimSequence: ...
    def ref_skel() -> SkeletonResource: ...
    def log_brief() -> None: ...

class AnimSequence:
    def get_num_soa_tracks() -> int: ...
    def get_num_tracks() -> int: ...
    def log_brief() -> None: ...
    def get_animation_pose(pose_data: VoidPtr, extract_context: VoidPtr) -> None: ...
```

**使用示例：**

```python
anim_seq_res = project.import_anim_sequence("model.gltf")
anim_seq_res.ref_skel = skeleton

# 获取动画信息
anim_seq = anim_seq_res.ref_seq()
print(f"Tracks: {anim_seq.get_num_tracks()}")
print(f"SOA Tracks: {anim_seq.get_num_soa_tracks()}")
```

#### AnimGraphResource - 动画图资源

```python
class AnimGraphResource(Resource):
    def create_simple_anim_graph(anim_seq: AnimSequenceResource) -> bool: ...
```

**使用示例：**

```python
anim_graph = re.world.AnimGraphResource()
success = anim_graph.create_simple_anim_graph(anim_seq_res)
if not success:
    raise RuntimeError("Failed to create animation graph")
```

**⚠️ 当前限制：** 目前只支持简单的单动画播放器。缺少以下接口：
- 手动创建和连接动画节点
- 混合多个动画
- 状态机控制

#### SkelMeshResource - 骨骼网格资源

```python
class SkelMeshResource(Resource):
    def GetSkinResource() -> SkinResource: ...
    def ref_skin() -> SkinResource: ...
    def ref_skeleton() -> SkeletonResource: ...
    def ref_anim_graph() -> AnimGraphResource: ...
```

**使用示例：**

```python
skel_mesh = re.world.SkelMeshResource()
skel_mesh.ref_skin = skin
skel_mesh.ref_skeleton = skeleton
skel_mesh.ref_anim_graph = anim_graph
```

### 组件类 (Components)

#### SkelMeshComponent - 骨骼网格组件

```python
class SkelMeshComponent(Component):
    def SetRefSkelMesh(skel_mesh: SkelMeshResource) -> None: ...
    def tick(delta_time: float) -> None: ...
    def update_render() -> None: ...
    def remove_object() -> None: ...
    def GetRuntimeMesh() -> MeshResource: ...
    def IsEnabled() -> bool: ...
```

**使用示例：**

```python
# 创建组件
skelmesh_comp = re.world.SkelMeshComponent(
    entity.add_component("SkelMeshComponent")
)
skelmesh_comp.SetRefSkelMesh(skel_mesh)

# 初始化动画系统（关键！）
skelmesh_comp.tick(0.0)

# 动画循环
while running:
    # 更新动画
    skelmesh_comp.tick(delta_time)
    
    # 更新渲染
    skelmesh_comp.update_render()
```

### 项目导入接口

```python
class Project:
    def import_skeleton(path: str) -> SkeletonResource: ...
    def import_skin(path: str) -> SkinResource: ...
    def import_anim_sequence(path: str) -> AnimSequenceResource: ...
    def import_mesh(path: str) -> MeshResource: ...
    def import_texture(path: str, mip_level: uint, to_vt: bool) -> TextureResource: ...
    def import_material(path: str) -> MaterialResource: ...
```

## 资源依赖关系

```
AnimSequenceResource ─┐
                      │
SkeletonResource ─────┼──> SkelMeshResource ──> SkelMeshComponent
                      │        ↑
SkinResource ─────────┘        │
    │                          │
    └──> MeshResource ─────────┘

AnimGraphResource ────> AnimInstance (Runtime)
```

**加载顺序：**
1. Mesh（网格）
2. Skeleton（骨骼）
3. Skin（蒙皮，依赖 Mesh 和 Skeleton）
4. AnimSequence（动画序列，依赖 Skeleton）
5. AnimGraph（动画图，依赖 AnimSequence）
6. SkelMeshResource（骨骼网格，组合以上所有）

## 完整工作流程

### 1. 初始化阶段

```python
# 创建应用
app = rbc.app.App()

# 初始化各子系统
app.init_ctx()           # 初始化 World 系统
app.init_device("dx")    # 初始化图形设备
lc.init()                # 初始化 Luisa Compute
app.init_render()        # 初始化渲染系统

# 初始化 World
app.init_world(project_path / "library")

# 创建项目
app._project = re.world.Project()
app._project.init(str(project_path / "assets"))
app._project.scan_project()

# 创建场景
app._scene = re.world.Scene()
app._scene.install()
```

### 2. 资源加载阶段

```python
# 分层加载资源
skeleton = project.import_skeleton("model.gltf")
mesh = project.import_mesh("model.gltf")

skin = project.import_skin("model.gltf")
skin.ref_skel = skeleton
skin.ref_mesh = mesh
skin.generate_LUT()

anim_seq = project.import_anim_sequence("model.gltf")
anim_seq.ref_skel = skeleton

anim_graph = re.world.AnimGraphResource()
anim_graph.create_simple_anim_graph(anim_seq)

skel_mesh = re.world.SkelMeshResource()
skel_mesh.ref_skin = skin
skel_mesh.ref_skeleton = skeleton
skel_mesh.ref_anim_graph = anim_graph
```

### 3. 实体创建阶段

```python
# 创建实体
entity = scene.add_entity()
entity.set_name("character")

# Transform
transform = re.world.TransformComponent(
    entity.add_component("TransformComponent")
)
transform.set_pos(lc.double3(0, 0, 0), True)
transform.set_scale(lc.double3(0.2, 0.2, 0.2), True)

# RenderComponent（必需）
render = re.world.RenderComponent(
    entity.add_component("RenderComponent")
)

# SkelMeshComponent
skelmesh = re.world.SkelMeshComponent(
    entity.add_component("SkelMeshComponent")
)
skelmesh.SetRefSkelMesh(skel_mesh)

# 关键：初始化动画系统
skelmesh.tick(0.0)
```

### 4. 动画更新阶段

```python
def animation_loop():
    # 计算 delta_time
    delta_time = 1.0 / 60.0
    
    # 更新动画
    skelmesh.tick(delta_time)
    
    # 更新渲染状态
    skelmesh.update_render()
```

## 缺失接口与建议

### 高优先级（物理动画必需）

#### 1. 运行时骨骼访问

**缺失接口：**
```python
# SkelMeshComponent 应该提供：
def get_current_bone_transforms() -> Vector[float4x4]: ...
def get_bone_transform(bone_index: int) -> float4x4: ...
def set_bone_transform(bone_index: int, transform: float4x4) -> None: ...
```

**用途：** 物理动画需要在运行时读取和修改骨骼变换。

**当前替代方案：** 通过骨骼层级导出接口（`get_joint_rest_poses`）获取初始姿态，物理系统在此基础上计算。

#### 2. 动画播放控制

**缺失接口：**
```python
# SkelMeshComponent 应该提供：
def play_animation(anim_seq: AnimSequenceResource) -> None: ...
def stop_animation() -> None: ...
def pause_animation() -> None: ...
def set_animation_time(time: float) -> None: ...
def get_animation_time() -> float: ...
def set_loop(loop: bool) -> None: ...
def set_playback_speed(speed: float) -> None: ...
```

**当前替代方案：** 只能使用创建时的动画图，无法动态切换。

#### 3. 动画混合

**缺失接口：**
```python
# AnimGraphResource 应该提供：
def create_blend_node(anim_seq1: AnimSequenceResource, 
                     anim_seq2: AnimSequenceResource) -> int: ...
def set_blend_weight(node_id: int, weight: float) -> None: ...
```

### 中优先级

#### 4. 程序化创建动画

**缺失接口：**
```python
# AnimSequenceBuilder 类（类似 MeshBuilder）
class AnimSequenceBuilder:
    def __init__(self, skeleton: SkeletonResource): ...
    def add_keyframe(self, joint_index: int, time: float, 
                     translation: float3, rotation: quat, scale: float3) -> None: ...
    def build(self) -> AnimSequenceResource: ...
```

#### 5. 程序化创建骨骼

**缺失接口：**
```python
# SkeletonBuilder 类
class SkeletonBuilder:
    def __init__(self): ...
    def add_joint(self, name: str, parent_index: int, 
                  local_transform: float4x4) -> int: ...
    def build(self) -> SkeletonResource: ...
```

#### 6. 动画事件

**缺失接口：**
```python
# 动画事件回调
def on_animation_event(callback: Callable[[str], None]) -> None: ...
```

### 低优先级

#### 7. IK (Inverse Kinematics)

**缺失接口：**
```python
def enable_ik() -> None: ...
def set_ik_target(effector_name: str, target_pos: float3) -> None: ...
```

## 物理动画集成最佳实践

### 1. 骨骼数据准备

```python
# 导出骨骼层级用于物理初始化
skeleton = project.import_skeleton("ragdoll.gltf")

bone_info = {
    'names': skeleton.get_joint_names(),
    'parents': skeleton.get_joint_parents(),
    'rest_poses': skeleton.get_joint_rest_poses()
}

# 保存供物理系统使用
import json
with open('bone_hierarchy.json', 'w') as f:
    json.dump(bone_info, f, default=lambda x: x.tolist() if hasattr(x, 'tolist') else x)
```

### 2. 物理骨骼链设置

```python
# 物理系统使用导出的骨骼数据
class PhysicsRagdoll:
    def __init__(self, bone_hierarchy):
        self.bones = []
        for i, (name, parent, rest_pose) in enumerate(zip(
            bone_hierarchy['names'],
            bone_hierarchy['parents'],
            bone_hierarchy['rest_poses']
        )):
            bone = PhysicsBone(name, parent, rest_pose)
            self.bones.append(bone)
    
    def update_visual_bones(self, skelmesh_component):
        """将物理结果写回渲染骨骼"""
        for i, bone in enumerate(self.bones):
            transform = bone.get_world_transform()
            # 缺失接口：skelmesh_component.set_bone_transform(i, transform)
            pass
```

### 3. 混合动画和物理

```python
class HybridAnimation:
    def __init__(self, skelmesh_comp):
        self.skelmesh = skelmesh_comp
        self.physics_bones = set()  # 受物理控制的骨骼索引
        self.animation_bones = set()  # 受动画控制的骨骼索引
    
    def tick(self, delta_time):
        # 更新动画
        self.skelmesh.tick(delta_time)
        
        # 获取动画姿态
        # 缺失接口：animated_poses = self.skelmesh.get_current_bone_transforms()
        
        # 物理系统更新
        for bone_idx in self.physics_bones:
            # 物理计算...
            physics_transform = self.simulate_bone(bone_idx)
            # 缺失接口：self.skelmesh.set_bone_transform(bone_idx, physics_transform)
            pass
```

## 调试技巧

### 1. 打印骨骼层级

```python
def print_bone_tree(skeleton, max_depth=5):
    names = skeleton.get_joint_names()
    parents = skeleton.get_joint_parents()
    
    def print_recursive(idx, depth):
        if depth > max_depth:
            return
        indent = "  " * depth
        parent_name = names[parents[idx]] if parents[idx] >= 0 else "root"
        print(f"{indent}└─ [{idx}] {names[idx]} (parent: {parent_name})")
        
        # Find children
        for i, p in enumerate(parents):
            if p == idx:
                print_recursive(i, depth + 1)
    
    # Print from roots
    for i, p in enumerate(parents):
        if p < 0:
            print_recursive(i, 0)

# 使用
print_bone_tree(skeleton)
```

### 2. 验证资源加载

```python
def validate_character_resources(skeleton, skin, anim_seq, skel_mesh):
    """验证所有资源是否正确加载和关联"""
    errors = []
    
    # Check skeleton
    if not skeleton:
        errors.append("Skeleton is None")
    elif skeleton.get_num_joints() == 0:
        errors.append("Skeleton has no joints")
    
    # Check skin
    if not skin:
        errors.append("Skin is None")
    else:
        if not skin.ref_skel():
            errors.append("Skin has no skeleton reference")
        if not skin.ref_mesh():
            errors.append("Skin has no mesh reference")
        if len(skin.JointRemaps()) == 0:
            errors.append("Skin has no joint remaps (generate_LUT not called?)")
    
    # Check animation
    if not anim_seq:
        errors.append("Animation sequence is None")
    elif not anim_seq.ref_skel():
        errors.append("Animation has no skeleton reference")
    
    # Check skel mesh
    if not skel_mesh:
        errors.append("SkelMeshResource is None")
    else:
        if not skel_mesh.ref_skin():
            errors.append("SkelMesh has no skin")
        if not skel_mesh.ref_skeleton():
            errors.append("SkelMesh has no skeleton")
        if not skel_mesh.ref_anim_graph():
            errors.append("SkelMesh has no anim graph")
    
    if errors:
        print("Resource validation failed:")
        for e in errors:
            print(f"  ✗ {e}")
        return False
    
    print("✓ All resources validated successfully")
    return True
```

### 3. 性能监控

```python
import time

class AnimationProfiler:
    def __init__(self):
        self.times = {
            'tick': [],
            'update_render': []
        }
    
    def profile_tick(self, skelmesh, delta_time):
        start = time.perf_counter()
        skelmesh.tick(delta_time)
        elapsed = time.perf_counter() - start
        self.times['tick'].append(elapsed)
        
        # Keep last 100 frames
        if len(self.times['tick']) > 100:
            self.times['tick'].pop(0)
    
    def report(self):
        for name, times in self.times.items():
            if times:
                avg = sum(times) / len(times) * 1000  # ms
                max_t = max(times) * 1000
                print(f"{name}: avg={avg:.2f}ms, max={max_t:.2f}ms")
```

## 常见问题 (FAQ)

### Q: 动画没有播放？

**A:** 检查以下几点：
1. 是否调用了 `skelmesh_comp.tick(0.0)` 进行初始化？
2. `skin.generate_LUT()` 是否被调用？
3. 资源引用是否正确设置（`skin.ref_skel = skeleton` 等）？
4. 每帧是否调用了 `tick(delta_time)`？

### Q: 模型不显示？

**A:** 检查：
1. 是否添加了 `RenderComponent`？
2. 材质是否正确设置？
3. 变换组件的 scale 是否合适（GLTF 模型通常需要 0.2 左右的 scale）？

### Q: 骨骼动画和物理如何结合？

**A:** 当前 Python 接口有限，建议：
1. 使用新的骨骼导出接口获取层级和休息姿态
2. 在 Python 中实现物理计算
3. 等待后续接口更新来支持运行时骨骼修改

### Q: 如何切换动画？

**A:** 当前 `create_simple_anim_graph` 只支持单动画。需要：
1. 创建新的 `AnimGraphResource`
2. 调用 `create_simple_anim_graph` 传入新动画
3. 更新 `skel_mesh.ref_anim_graph = new_graph`

或者等待后续支持动画混合和状态机的接口。

## 参考资源

- **C++ 示例**: `rbc/tests/sample_anim/main.cpp`
- **Python 完整示例**: `samples/complete_anim_example.py`
- **资源检查工具**: `samples/gltf_anim_inspector.py`
- **C++ 头文件**: 
  - `rbc_anim/skeletal_mesh.h`
  - `rbc_world/resources/skeleton.h`
  - `rbc_world/components/skelmesh_component.h`

## 更新日志

### 2024-XX-XX
- 新增骨骼层级导出接口（`get_joint_names`, `get_joint_parents`, `get_joint_rest_poses` 等）
- 添加 `complete_anim_example.py` 完整示例
- 添加本说明文档

### 计划更新
- [ ] 运行时骨骼变换访问接口
- [ ] 动画播放控制接口
- [ ] 程序化创建动画/骨骼接口
- [ ] IK 支持
