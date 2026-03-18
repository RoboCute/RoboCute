# 骨骼动画场景基础教程

本教程介绍如何在 RoboCute 中使用 Python 创建和渲染骨骼动画场景。

## 前置条件

- 已安装 RoboCute 并配置好开发环境
- 了解基本的 Python 编程
- 有一个包含骨骼动画资源的 RBC 项目（包含 `test_anim.gltf` 文件）

## 完整示例代码

参考示例：`samples/app_anim_scene.py`

```bash
# 运行示例（从项目根目录）
uv run python -m samples.app_anim_scene -p <project_path> -b dx

# 或者使用 Python 模块方式
uv run python -c "import sys; sys.path.insert(0, '.'); import samples.app_anim_scene; samples.app_anim_scene.main()" -- -p <project_path> -b dx
```

## 步骤详解

### 1. 导入必要的模块

```python
import robocute as rbc
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
import numpy as np
```

- `robocute` - 主应用模块
- `robocute.rbc_ext` - 扩展模块，包含 World 系统
- `robocute.rbc_ext.luisa` - Luisa 计算库绑定

### 2. 初始化应用

```python
app = rbc.app.App()
app.init(project_path=project_path, backend_name="dx")
```

参数说明：
- `project_path` - RBC 项目路径（包含 `rbc_project.json` 的目录）
- `backend_name` - 图形后端，可选 `"dx"` (DirectX) 或 `"vk"` (Vulkan)

### 3. 初始化显示

```python
resolution = lc.uint2(1024, 1024)
app.init_display(resolution.x, resolution.y)

# 配置相机
app.display_cam.set_fov(np.radians(60.0))
app.display_cam.set_near_plane(0.01)
app.display_cam.set_far_plane(1000.0)

# 启用相机控制
app.ctx.enable_camera_control()
```

### 4. 从 GLTF 加载骨骼动画资源

GLTF 文件包含骨骼动画的多个组成部分，需要分别导入：

```python
# 获取 GLTF 文件的相对路径（相对于项目根目录）
gltf_path = "anim_test/test_anim.gltf"

# Step 1: 导入网格
mesh = app._project.import_mesh(gltf_path)

# Step 2: 导入骨骼
skeleton = app._project.import_skeleton(gltf_path)

# Step 3: 导入皮肤（依赖骨骼和网格）
skin = app._project.import_skin(gltf_path)
skin.ref_skel = skeleton
skin.ref_mesh = mesh

# Step 4: 导入动画序列（依赖骨骼）
anim_seq = app._project.import_anim_sequence(gltf_path)
anim_seq.ref_skel = skeleton

# Step 5: 创建动画图
anim_graph = re.world.AnimGraphResource()
anim_graph.create_simple_anim_graph(anim_seq)

# Step 6: 创建 SkelMeshResource（依赖皮肤、骨骼、动画图）
skel_mesh = re.world.SkelMeshResource()
skel_mesh.ref_skin = skin
skel_mesh.ref_skeleton = skeleton
skel_mesh.ref_anim_graph = anim_graph
```

### 5. 创建带 SkelMeshComponent 的实体

```python
# 创建实体
entity = app.scene.add_entity()
entity.set_name("animated_character")

# 添加 TransformComponent
transform = re.world.TransformComponent(
    entity.add_component("TransformComponent")
)
transform.set_pos(lc.double3(0, 0, 0), True)
transform.set_scale(lc.double3(0.2, 0.2, 0.2), True)

# 添加 RenderComponent（SkelMeshComponent 需要）
render = re.world.RenderComponent(
    entity.add_component("RenderComponent")
)

# 添加 SkelMeshComponent
skelmesh_comp = re.world.SkelMeshComponent(
    entity.add_component("SkelMeshComponent")
)
skelmesh_comp.SetRefSkelMesh(skel_mesh)

# 初始化动画
skelmesh_comp.tick(0.0)
```

### 6. 动画更新循环

```python
def tick_animation(entity, delta_time: float):
    """更新动画"""
    if not entity:
        return
    
    try:
        skelmesh = re.world.SkelMeshComponent(
            entity.get_component("SkelMeshComponent")
        )
        if skelmesh:
            skelmesh.tick(delta_time)
    except Exception as e:
        pass  # 组件可能不存在

def update_render(entity):
    """更新渲染状态"""
    if not entity:
        return
    
    try:
        skelmesh = re.world.SkelMeshComponent(
            entity.get_component("SkelMeshComponent")
        )
        if not skelmesh or not skelmesh.IsEnabled():
            return
        
        skelmesh.update_render()
    except Exception as e:
        pass
```

### 7. 设置主循环

```python
last_time = time.time()

def tick_logic():
    nonlocal last_time
    
    cur_time = time.time()
    delta_time = cur_time - last_time
    last_time = cur_time
    
    # 更新动画
    tick_animation(entity, delta_time)
    
    # 更新渲染
    update_render(entity)

app.set_user_callback(tick_logic)
app.run()
```

## Project API 参考（骨骼动画相关）

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `import_mesh` | `path: str` | `MeshResource` | 从 GLTF 导入网格 |
| `import_skeleton` | `path: str` | `SkeletonResource` | 从 GLTF 导入骨骼 |
| `import_skin` | `path: str` | `SkinResource` | 从 GLTF 导入皮肤 |
| `import_anim_sequence` | `path: str` | `AnimSequenceResource` | 从 GLTF 导入动画序列 |
| `import_texture` | `path: str, mip_level: int, to_vt: bool` | `TextureResource` | 导入纹理 |
| `import_scene` | `path: str, extra_meta: str` | `Scene` | 导入 RBC 场景文件（.rbc_scene） |

## AnimGraphResource API 参考

### 方法

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `create_simple_anim_graph` | `anim_seq: AnimSequenceResource` | `bool` | 创建简单的动画图（根节点 + 序列播放器） |

### 使用说明

`create_simple_anim_graph` 方法创建一个最简单的动画图，包含：
- 一个根节点（`AnimNode_Root`）
- 一个序列播放器节点（`AnimNode_SequencePlayer`）
- 根节点的输出连接到序列播放器

适用于单个动画循环播放的场景。

## SkelMeshComponent API 参考

### 方法

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `SetRefSkelMesh` | `skel_mesh: SkelMeshResource` | `None` | 设置骨骼网格资源 |
| `tick` | `delta_time: float` | `None` | 更新动画状态 |
| `update_render` | - | `None` | 更新渲染状态 |
| `remove_object` | - | `None` | 移除渲染对象 |
| `GetRuntimeMesh` | - | `MeshResource` | 获取运行时变形后的网格 |
| `IsEnabled` | - | `bool` | 检查动画是否启用 |

### 使用注意事项

1. **RenderComponent 是必需的** - SkelMeshComponent 需要 RenderComponent 才能正确渲染
2. **初始化调用** - 首次使用前应调用 `tick(0.0)` 初始化动画系统
3. **双缓冲更新** - `tick()` 和 `update_render()` 需要分别在游戏线程和渲染线程调用

## 资源加载说明

### 支持的动画资源格式

RoboCute 支持通过 GLTF/GLB 文件导入骨骼动画。与静态资源不同，骨骼动画需要分别导入多个组件：

```python
# 导入骨骼动画的各个组成部分
mesh = app._project.import_mesh("anim_test/model.gltf")
skeleton = app._project.import_skeleton("anim_test/model.gltf")
skin = app._project.import_skin("anim_test/model.gltf")
anim_seq = app._project.import_anim_sequence("anim_test/model.gltf")

# 注意：import_scene 用于导入 RBC 场景文件（.rbc_scene），不是 GLTF
# scene = app._project.import_scene("scene.rbc_scene", "{}")
```

### 资源路径

资源路径是相对于项目根目录的：

```python
# 如果文件位于 <project>/assets/anim_test/model.gltf
gltf_path = "assets/anim_test/model.gltf"
mesh = app._project.import_mesh(gltf_path)
```

### 资源依赖关系

骨骼动画资源之间存在依赖关系：

1. **Skin** 依赖 **Skeleton** 和 **Mesh**
   ```python
   skin.ref_skel = skeleton
   skin.ref_mesh = mesh
   ```

2. **AnimSequence** 依赖 **Skeleton**
   ```python
   anim_seq.ref_skel = skeleton
   ```

3. **SkelMeshResource** 依赖 **Skin**、**Skeleton** 和 **AnimGraph**
   ```python
   skel_mesh.ref_skin = skin
   skel_mesh.ref_skeleton = skeleton
   skel_mesh.ref_anim_graph = anim_graph
   ```

## 故障排除

### 问题：无法导入骨骼动画资源

**解决方案：**
- 确保 GLTF 文件包含骨骼动画数据
- 检查资源路径是否正确（相对于项目根目录）
- 验证 GLTF 文件格式是否正确

### 问题：无法找到 SkelMeshComponent

**解决方案：**
- 确保实体已添加 SkelMeshComponent
- 检查是否正确调用了 `entity.add_component("SkelMeshComponent")`

### 问题：动画不播放

**解决方案：**
- 确保调用了 `skelmesh_comp.tick(0.0)` 进行初始化
- 检查 `tick()` 是否在每帧被调用
- 验证 `SkelMeshResource` 是否正确设置（特别是 `ref_anim_graph`）
- 检查资源依赖关系是否正确设置

### 问题：模型不显示

**解决方案：**
- 确保添加了 RenderComponent
- 检查相机位置和朝向
- 验证光照设置（添加天空盒或默认光源）
- 检查 `update_render()` 是否在每帧被调用

## 进阶主题

### 手动创建动画图

对于简单的动画播放，使用 `create_simple_anim_graph` 方法：

```python
# 创建动画图资源
anim_graph = re.world.AnimGraphResource()

# 从 GLTF 导入动画序列
anim_seq = app._project.import_anim_sequence("anim_test/model.gltf")
anim_seq.ref_skel = skeleton

# 创建简单的动画图（包含一个根节点和一个序列播放器）
anim_graph.create_simple_anim_graph(anim_seq)

# 创建骨骼网格资源
skel_mesh = re.world.SkelMeshResource()
skel_mesh.ref_skeleton = skeleton
skel_mesh.ref_skin = skin
skel_mesh.ref_anim_graph = anim_graph
```

对于更复杂的动画控制（如混合多个动画），需要手动配置动画图节点。

### 运行时切换动画

```python
# 获取 SkelMeshComponent
skelmesh = re.world.SkelMeshComponent(entity.get_component("SkelMeshComponent"))

# 切换骨骼网格资源
skelmesh.SetRefSkelMesh(new_skel_mesh)

# 重新初始化
skelmesh.tick(0.0)
```

## 参考

- [World Pybind 文档](../dev/world/world_pybind.md)
- [骨骼动画系统文档](../dev/world/skeletal_anim.md)
- C++ 示例：`rbc/tests/sample_anim/main.cpp`
