# Welcome to RoboCute

<div style="text-align: center;" markdown="1">

**Make Robotics Cute!**

A Python-first 3D AIGC/Robotics development tool with node-based workflow

[![Python](https://img.shields.io/badge/Python-3.13+-blue.svg)](https://www.python.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0.html)
[![Status](https://img.shields.io/badge/Status-Early%20Development-orange.svg)](devlog/README.md)

</div>

---

## What is RoboCute? / 什么是 RoboCute?

**English:**

RoboCute is a Python-first 3D AIGC (AI-Generated Content) and Robotics development tool that adopts a ComfyUI-like node-based server-client architecture. It comes with a self-developed cross-platform graphics engine, runtime, and editor, allowing you to write node algorithms directly in Python, seamlessly integrating with the entire Python ecosystem. The desktop runtime overcomes the inherent limitations of web platforms regarding files and 3D content, providing more convenient interactions and better performance.

**中文:**

RoboCute 是一个 Python-first 的 3D AIGC（AI生成内容）和机器人开发工具，采用类似 ComfyUI 的节点式 server-client 架构。它配备了自研的跨平台图形引擎、运行时和编辑器，允许您直接用 Python 编写节点算法，无缝接入整个 Python 生态。桌面端运行时摆脱了 Web 平台对文件和 3D 内容的天然局限，提供更便利的交互和更好的性能。

## Key Features / 核心特性

**English:**

- 🐍 **Python-First Architecture**: All core logic runs in Python, with optional C++ editor for visualization
- 🎨 **Node-Based Workflow**: Visual node graph editing similar to ComfyUI for algorithm composition
- 🎬 **Scene Management**: Complete ECS (Entity-Component-System) based scene management
- 🎭 **Animation System**: Rich animation system with keyframe support and timeline playback
- 🔬 **Physics Simulation**: Integration with UIPC for rigid body physics simulation
- 🤖 **AIGC Nodes**: Built-in nodes for text2image, text2model, text2anim workflows
- 🖥️ **Cross-Platform Editor**: Native desktop editor built with Qt 6 and LuisaCompute
- 🔌 **Extensible**: Easy to create custom nodes and extend editor functionality
- 🚀 **Headless Mode**: Support for offline rendering and large-scale simulations without GUI

**中文:**

- 🐍 **Python-First 架构**: 所有核心逻辑在 Python 中运行，可选的 C++ 编辑器用于可视化
- 🎨 **节点式工作流**: 类似 ComfyUI 的可视化节点图编辑，用于算法组合
- 🎬 **场景管理**: 完整的基于 ECS（实体-组件-系统）的场景管理
- 🎭 **动画系统**: 丰富的动画系统，支持关键帧和时间轴播放
- 🔬 **物理模拟**: 集成 UIPC 进行刚体物理模拟
- 🤖 **AIGC 节点**: 内置 text2image、text2model、text2anim 等节点
- 🖥️ **跨平台编辑器**: 使用 Qt 6 和 LuisaCompute 构建的原生桌面编辑器
- 🔌 **可扩展**: 易于创建自定义节点和扩展编辑器功能
- 🚀 **无头模式**: 支持无 GUI 的离线渲染和大规模仿真


## 🚀 Quick Start / 快速开始

### Installation / 安装

WIP (当前还不能使用)

```bash
pip install robocute
```

### Basic Usage / 基本使用

运行一个服务器

```python
import robocute as rbc

mesh_path = "D:/ws/data/assets/models/bunny.obj"
# Create a scene
scene = rbc.Scene()
scene.start()

# Create an entity
mesh_id = scene.load_mesh(mesh_path, priority=rbc.LoadPriority.High)
robot = scene.create_entity("Robot")
# Add tranform component to define its position
scene.add_component(
    robot.id,
    "transform",
    rbc.TransformComponent(
        position=[0.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0, 1.0],
        scale=[1.0, 1.0, 1.0],
    ),
)
# Add render component with mesh reference
render_comp = rbc.RenderComponent(
    mesh_id=mesh_id,
    material_ids=[],
)
scene.add_component(robot1.id, "render", render_comp)

# Start server with editor service
server = rbc.Server(title="RoboCute Server", version="0.2.0")
editor_service = rbc.EditorService(scene)
server.register_service(editor_service)
server.start(port=5555)

# Now you can connect with the C++ editor
print("Server started on port 5555")
print("Start the editor to connect and visualize the scene")
```

打开`rbc_editor`，查看场景并连接节点，执行后在Editor上查看结果

![rbc_editor](images/RBCEditor.png)

### Example: Creating Animation Nodes / 示例：创建动画节点

Headless模式下，可以完全不使用Editor来定义节点连接，这一部分同样也可以通过预先保存的节点图进行执行。方便在远程linux服务器上进行大批量的仿真/AI计算。

```python
import robocute as rbc
import custom_nodes.animation_nodes as animation_nodes

# Build a rotation animation graph
graph_def = rbc.GraphDefinition(
    nodes=[
        rbc.NodeDefinition(
            node_id="entity_input",
            node_type="entity_input",
            inputs={"entity_id": robot.id},
        ),
        rbc.NodeDefinition(
            node_id="rotation_anim",
            node_type="rotation_animation",
            inputs={
                "radius": 2.0,
                "angular_velocity": 1.0,
                "duration_frames": 120,
                "fps": 30.0,
            },
        ),
        rbc.NodeDefinition(
            node_id="anim_output",
            node_type="animation_output",
            inputs={"name": "rotation_test", "fps": 30.0},
        ),
    ],
    connections=[
        rbc.NodeConnection(
            from_node="entity_input",
            from_output="entity",
            to_node="rotation_anim",
            to_input="entity",
        ),
        rbc.NodeConnection(
            from_node="rotation_anim",
            from_output="animation",
            to_node="anim_output",
            to_input="animation",
        ),
    ],
)

# Execute the graph
scene_context = rbc.SceneContext(scene)
graph = rbc.NodeGraph.from_definition(graph_def, "test_graph", scene_context)
executor = rbc.GraphExecutor(graph, scene_context)
result = executor.execute()
```

## Documentation / 文档

- 📖 [Getting Started](getting-started/overview.md) 概览
- 🛠️ [Build Guide](BUILD.md) 构建指南
- 💻 [Developer Guide](DeveloperGuide.md) 开发者指南
- 📝 [Development Log](devlog/README.md) 开发文档
- 🎨 [Architecture](design/Architecture.md) 架构入口

## Project Status / 项目状态

RoboCute is currently in **early development**. See [Development Log](devlog/README.md) for progress.

RoboCute 目前处于**早期开发阶段**。查看[开发日志](devlog/README.md)了解进度。

