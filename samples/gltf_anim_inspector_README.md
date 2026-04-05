# GLTF骨骼动画资源检查工具

## 简介

这个示例展示了如何从Python侧读取GLTF文件中的骨骼动画相关资源，并打印详细信息。

## 功能

该工具可以检查并打印以下资源信息：

1. **骨架 (Skeleton)** - 骨骼层级结构
2. **皮肤/蒙皮 (Skin)** - 骨骼绑定信息、逆绑定姿态矩阵、Joint重映射
3. **动画序列 (Animation Sequence)** - 动画轨道信息
4. **动画图 (Animation Graph)** - 动画状态机
5. **骨骼网格 (SkelMesh)** - 骨骼动画网格资源

## 使用方式

```bash
cd <project_root>
uv run python -m samples.gltf_anim_inspector -p <project_path> -g <gltf_file>
```

### 示例

```bash
# 检查anim_test项目中的test_anim.gltf
uv run python -m samples.gltf_anim_inspector \
    -p d:/ws/repos/RoboCute-repo/rbc-project-anim \
    -g assets/anim_test/test_anim.gltf
```

### 参数说明

- `-p, --project`: RBC项目路径 (包含 `rbc_project.json` 的目录)
- `-g, --gltf`: GLTF文件路径 (相对于项目 `assets` 目录的相对路径)

## 示例输出

```
======================================================================
 蒙皮 (Skin) 信息
======================================================================
资源类型: SkinResource
资源有效: True

--- 关联资源 ---
  骨架: <rbc.world.SkeletonResource object>
  网格: <rbc.world.MeshResource object>

--- Joint重映射信息 ---
  Joint数量: 24

  Joint名称列表 (前10个):
    [0] Root
    [1] Spine
    [2] Chest
    [3] Neck
    [4] Head
    [5] LeftShoulder
    [6] LeftArm
    [7] LeftForeArm
    [8] LeftHand
    [9] RightShoulder
    ... 还有 14 个Joint

--- 逆绑定姿态 (Inverse Bind Poses) ---
  矩阵数量: 24
  预期数量: 24 (每个Joint一个矩阵)

  前3个Joint的逆绑定矩阵:
    [0] Root:
        [1.0000, 0.0000, 0.0000, 0.0000]
        [0.0000, 1.0000, 0.0000, 0.0000]
        [0.0000, 0.0000, 1.0000, 0.0000]
        [0.0000, 0.0000, 0.0000, 1.0000]
    ...

调用 log_brief() 输出皮肤信息:
[Skin日志输出]
```

## 当前API限制

### 已实现的接口

当前Python绑定已经提供了以下基础接口：

**SkeletonResource:**
- `ref_skel()` - 获取底层骨架引用 (返回VoidPtr)
- `log_brief()` - 打印简要信息

**SkinResource:**
- `ref_skel()` / `ref_mesh()` - 获取关联资源
- `JointRemaps()` - 获取Joint名称列表
- `InverseBindPoses()` - 获取逆绑定姿态矩阵数组
- `JointRemapsLUT()` - 获取Joint重映射查找表
- `generate_LUT()` - 生成查找表
- `log_brief()` - 打印简要信息

**AnimSequenceResource:**
- `ref_seq()` - 获取动画序列对象
- `ref_skel()` - 获取关联骨架
- `log_brief()` - 打印简要信息

**AnimSequence:**
- `get_num_soa_tracks()` - 获取SOA轨道数量
- `get_num_tracks()` - 获取总轨道数量
- `get_animation_pose()` - 获取动画姿态 (需要VoidPtr参数)

**AnimGraphResource:**
- `create_simple_anim_graph(anim_seq)` - 创建简单动画图

**SkelMeshResource:**
- `ref_skin()` / `ref_skeleton()` / `ref_anim_graph()` - 获取关联资源

### 需要扩展的接口

为了更好地支持程序化操作，建议扩展以下接口：

**SkeletonResource 建议扩展:**
```python
def get_joint_count() -> int: ...
def get_joint_name(index: int) -> str: ...
def get_joint_parent(index: int) -> int: ...
def get_joint_local_transform(index: int) -> float4x4: ...
def set_joint_local_transform(index: int, transform: float4x4) -> None: ...
def get_joint_world_transform(index: int) -> float4x4: ...
```

**AnimSequence/AnimSequenceResource 建议扩展:**
```python
def get_duration() -> float: ...
def get_keyframe_count(track: int) -> int: ...
def get_keyframe_time(track: int, keyframe_index: int) -> float: ...
def get_keyframe_value(track: int, keyframe_index: int) -> float3/quat: ...
def sample(time: float, skeleton: SkeletonResource) -> List[float4x4]: ...
```

**SkinResource 建议扩展:**
```python
def set_joint_remaps(names: List[str]) -> None: ...
def set_inverse_bind_poses(matrices: List[float4x4]) -> None: ...
```

## 后续开发建议

### 1. 程序化创建资源

参考 `mesh_builder.py` 的模式，创建以下Builder类：

```python
# skeleton_builder.py
class SkeletonBuilder:
    def __init__(self):
        self.joints = []
    
    def add_joint(self, name: str, parent_index: int, local_transform: float4x4):
        ...
    
    def build(self) -> SkeletonResource:
        ...

# anim_sequence_builder.py  
class AnimSequenceBuilder:
    def __init__(self, skeleton: SkeletonResource):
        self.skeleton = skeleton
        self.tracks = []
    
    def add_keyframe(self, joint_index: int, time: float, 
                     translation: float3, rotation: quat, scale: float3):
        ...
    
    def build(self) -> AnimSequenceResource:
        ...
```

### 2. 实时动画控制

扩展 `SkelMeshComponent` 接口：

```python
# 播放控制
def play_animation(anim_seq: AnimSequenceResource, loop: bool = True) -> None: ...
def pause_animation() -> None: ...
def stop_animation() -> None: ...
def set_animation_time(time: float) -> None: ...
def get_animation_time() -> float: ...

# 骨骼操作
def get_bone_transform(bone_index: int) -> float4x4: ...
def set_bone_transform(bone_index: int, transform: float4x4) -> None: ...
def get_bone_position(bone_index: int) -> float3: ...
def set_bone_position(bone_index: int, position: float3) -> None: ...
```

### 3. 调试工具

基于这个检查工具，可以进一步开发：

- **骨骼可视化** - 在场景中显示骨骼层级
- **动画预览** - 单步播放动画并查看pose
- **性能分析** - 统计动画采样和蒙皮耗时

## 参考

- C++示例: `rbc/tests/sample_anim/main.cpp`
- 动画场景示例: `samples/app_anim_scene.py`
- 网格构建器: `samples/mesh_builder.py`
