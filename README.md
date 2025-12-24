# RoboCute

<div align="center">

**Make Robotics Cute!**

A Python-first 3D AIGC/Robotics development tool with node-based workflow

[![Python](https://img.shields.io/badge/Python-3.12+-blue.svg)](https://www.python.org/)
[![License](https://img.shields.io/badge/License-LGPLv3-green.svg)](LICenses/LGPLv3.txt)
[![Status](https://img.shields.io/badge/Status-Early%20Development-orange.svg)](doc/devlog/)

</div>

---

## 📖 Overview / 概述

**English:**

RoboCute is a Python-first 3D AIGC (AI-Generated Content) and Robotics development tool that adopts a ComfyUI-like node-based server-client architecture. It comes with a self-developed cross-platform graphics engine, runtime, and editor, allowing you to write node algorithms directly in Python, seamlessly integrating with the entire Python ecosystem. The desktop runtime overcomes the inherent limitations of web platforms regarding files and 3D content, providing more convenient interactions and better performance.

**中文:**

RoboCute 是一个 Python-first 的 3D AIGC（AI生成内容）和机器人开发工具，采用类似 ComfyUI 的节点式 server-client 架构。它配备了自研的跨平台图形引擎、运行时和编辑器，允许您直接用 Python 编写节点算法，无缝接入整个 Python 生态。桌面端运行时摆脱了 Web 平台对文件和 3D 内容的天然局限，提供更便利的交互和更好的性能。

## ✨ Key Features / 核心特性

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
```python
import robocute as rbc

# Create a scene
scene = rbc.Scene()
scene.start()

# Create an entity
robot = scene.create_entity("Robot")
scene.add_component(
    robot.id,
    "transform",
    rbc.TransformComponent(
        position=[0.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0, 1.0],
        scale=[1.0, 1.0, 1.0],
    ),
)

# Start server with editor service
server = rbc.Server(title="RoboCute Server", version="0.1.0")
editor_service = rbc.EditorService(scene)
server.register_service(editor_service)
server.start(port=5555)

# Now you can connect with the C++ editor
print("Server started on port 5555")
print("Start the editor to connect and visualize the scene")
```

### Example: Creating Animation Nodes / 示例：创建动画节点


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

## 📚 Documentation / 文档

**English:**

- 📖 [Architecture Documentation](doc/design/Architecture.md) - System architecture overview
- 🛠️ [Build Guide](BUILD.md) - How to build from source
- 📝 [Development Log](doc/devlog/) - Development progress and milestones
- 🎨 [Design Documents](doc/design/) - Design decisions and specifications
- 💻 [Samples](samples/) - Example code and tutorials

**中文:**

- 📖 [架构文档](doc/design/Architecture.md) - 系统架构概览
- 🛠️ [构建指南](BUILD.md) - 如何从源码构建
- 📝 [开发日志](doc/devlog/) - 开发进度和里程碑
- 🎨 [设计文档](doc/design/) - 设计决策和规范
- 💻 [示例代码](samples/) - 示例代码和教程

## 🏗️ Project Status / 项目状态

**English:**

RoboCute is currently in **early development**. The following milestones have been completed:

- ✅ **v0.1 MVP** (Completed): Basic node system, scene management, animation workflow
- 🚧 **v0.2** (In Progress): Codebase refactoring, world_v2 scene system, improved editor

See [Development Log](doc/devlog/) for detailed progress.

**中文:**

RoboCute 目前处于**早期开发阶段**。已完成以下里程碑：

- ✅ **v0.1 MVP**（已完成）：基础节点系统、场景管理、动画工作流
- 🚧 **v0.2**（进行中）：代码库重构、world_v2 场景系统、改进的编辑器

详细进度请查看[开发日志](doc/devlog/)。

## 🤝 Contributing / 贡献

**English:**

We welcome contributions! Here's how you can get involved:

- 🐛 **Report Issues**: Found a bug? Open an issue on GitHub
- 💡 **Suggest Features**: Have an idea? Share it in discussions
- 📝 **Write Custom Nodes**: Create and share custom nodes for the community
- 🔧 **Extend Editor**: Build editor extensions and improvements
- 📚 **Improve Documentation**: Help us make the docs better

For building from source, see [BUILD.md](BUILD.md).

**中文:**

我们欢迎贡献！您可以通过以下方式参与：

- 🐛 **报告问题**: 发现 bug？在 GitHub 上提交 issue
- 💡 **建议功能**: 有想法？在讨论区分享
- 📝 **编写自定义节点**: 为社区创建和分享自定义节点
- 🔧 **扩展编辑器**: 构建编辑器扩展和改进
- 📚 **改进文档**: 帮助我们完善文档

从源码构建请参考 [BUILD.md](BUILD.md)。

## 📄 License / 许可证

**English:**

RoboCute is licensed under LGPLv3. See [Licenses/LGPLv3.txt](Licenses/LGPLv3.txt) for details.

### Third-Party Dependencies / 第三方依赖

- **Tracy v0.13.0**: Included as source code, 3-clause BSD license
- **Qt v6.9.3**: LGPLv3 license, see [Licenses/LGPLv3.txt](Licenses/LGPLv3.txt)

**中文:**

RoboCute 采用 LGPLv3 许可证。详情请参阅 [Licenses/LGPLv3.txt](Licenses/LGPLv3.txt)。

### 第三方依赖

- **Tracy v0.13.0**: 以源码形式引入，3-clause BSD 许可证
- **Qt v6.9.3**: LGPLv3 许可证，参见 [Licenses/LGPLv3.txt](Licenses/LGPLv3.txt)

## 🔗 Links / 链接

**English:**

- 📦 [PyPI Package](https://pypi.org/project/robocute/) (Coming soon)
- 📖 [Full Documentation](doc/DOC.md)
- 🐛 [Issue Tracker](https://github.com/your-org/robocute/issues)
- 💬 [Discussions](https://github.com/your-org/robocute/discussions)

**中文:**

- 📦 [PyPI 包](https://pypi.org/project/robocute/)（即将推出）
- 📖 [完整文档](doc/DOC.md)
- 🐛 [问题追踪](https://github.com/your-org/robocute/issues)
- 💬 [讨论区](https://github.com/your-org/robocute/discussions)

---

<div align="center">

Made with ❤️ by the RoboCute Team

</div>
